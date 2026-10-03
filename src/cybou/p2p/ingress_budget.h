// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_P2P_INGRESS_BUDGET_H
#define CYBOU_P2P_INGRESS_BUDGET_H
#include <chrono>
#include <array>
#include <memory>
#include <map>
#include <mutex>
#include <string>
namespace cybou::p2p {
/// \brief Локальная ограниченная политика допуска по ingress-нагрузке.
/// \details Это только anti-abuse лимитер CPU/байтов/конкурентных запросов.
///          Он не меняет каноническое состояние, AUTH, финализацию или eligibility для `Validation`.
/// \thread_safety `Admit()` и освобождение lease потокобезопасны; внутренняя синхронизация полностью инкапсулирована.
class IngressBudget {
    struct Transfers {
        /// \brief Защищает счетчики активных storage-передач.
        std::mutex mutex;
        /// \brief Число одновременных storage-запросов на IP-адрес.
        std::map<std::string, size_t> active;
        /// \brief Общее число одновременных storage-запросов на узле.
        size_t total{0};
    };
    struct TransferLease {
        /// \brief Общие счетчики, из которых lease освобождает слот при разрушении.
        std::shared_ptr<Transfers> state;
        /// \brief IP-адрес, для которого был захвачен слот.
        std::string address;
        /// \brief `true` только после успешного резервирования слота.
        bool acquired{false};
        /// \post При успешном захвате слот возвращается автоматически даже при исключениях.
        ~TransferLease() {
            if (!acquired) return;
            std::lock_guard lock(state->mutex);
            auto it = state->active.find(address);
            if (--it->second == 0) state->active.erase(it);
            --state->total;
        }
    };
public:
    /// \brief Категория входящей работы для раздельных квот.
    enum class Work {
        CONNECTION,    ///< Попытка установить новую P2P-сессию.
        OPERATION,     ///< Кандидат-операция и связанные relay payload'ы.
        STORAGE_PUT,   ///< Загрузка авторизованного encrypted chunk.
        STORAGE_GET,   ///< Выгрузка stored encrypted chunk.
        STORAGE_PROOF, ///< Выдача Merkle/durability proof без полной выгрузки chunk'а.
    };

    /// \brief Резервирует конкурентный слот для storage-передачи.
    /// \param address Числовой IP-адрес удаленной стороны.
    /// \return RAII-lease, либо пустой `shared_ptr`, если квота уже исчерпана.
    /// \post Один адрес может иметь не более `2` активных передач, а весь узел — не более `8`.
    std::shared_ptr<void> AcquireStorageTransfer(const std::string& address) {
        auto lease = std::make_shared<TransferLease>();
        lease->state = m_transfers;
        lease->address = address;
        std::lock_guard lock(m_transfers->mutex);
        if (m_transfers->total >= 8) return {};
        auto& active = m_transfers->active[address];
        if (active >= 2) return {};
        ++active;
        ++m_transfers->total;
        lease->acquired = true;
        return lease;
    }

    /// \brief Проверяет и учитывает очередную единицу входящей работы.
    /// \param address Числовой IP-адрес удаленной стороны.
    /// \param work Категория работы.
    /// \param bytes Объем байтов для byte-rate лимитов; для metadata-only запросов это верхняя оценка резервирования.
    /// \param now Точка времени для тестов и явного управления окнами.
    /// \return `true`, если работа допущена и квоты учтены; `false` при локальном отказе fail-closed.
    /// \details Счётчики ведутся в окне 60 секунд с посекундным refill без накопления неиспользованного кредита.
    bool Admit(const std::string& address, Work work, size_t bytes = 0,
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now())
    {
        std::lock_guard lock(m_mutex);

        // Очистка делается редко и под тем же lock: это удерживает O(1) путь
        // обычного допуска и не позволяет злоумышленнику раздувать карту peer'ов
        // быстрее, чем истекает минутное окно.
        if (now - m_last_cleanup >= std::chrono::seconds{1} || m_peers.size() >= 4096) {
            m_last_cleanup = now;
            for (auto it = m_peers.begin(); it != m_peers.end();) {
                if (now - it->second.start >= std::chrono::seconds{60}) {
                    it = m_peers.erase(it);
                } else {
                    ++it;
                }
            }
        }

        auto it = m_peers.find(address);
        if (it == m_peers.end()) {
            if (m_peers.size() >= 4096) return false;
            it = m_peers.emplace(address, Window{now}).first;
        } else if (now - it->second.start >= std::chrono::seconds{60}) {
            // Истекшее окно конкретного пира сбрасываем адресно, чтобы не
            // пересчитывать историю и не переносить кредит между окнами.
            it->second = Window{now};
        }

        auto& w = it->second;
        // Обнуляем только посекундные счетчики текущего окна. Полные
        // минутные лимиты остаются, чтобы пик на границе секунд не превращался
        // в бесплатный двойной burst.
        const auto second = std::chrono::duration_cast<std::chrono::seconds>(now - w.start).count();
        if (second != w.second) {
            w.second = second;
            w.operations_second = 0;
            w.bytes_second = 0;
            w.connections_second = 0;
            w.storage_second.fill(0);
            w.storage_bytes_second = 0;
        }
        if (work == Work::CONNECTION) {
            if (w.connections_second >= 4 || w.connections >= 60) return false;
            ++w.connections_second;
            ++w.connections;
            return true;
        }
        if (work != Work::OPERATION) {
            const auto kind = static_cast<size_t>(work) - static_cast<size_t>(Work::STORAGE_PUT);
            if (kind >= w.storage.size()) return false;
            const bool proof = work == Work::STORAGE_PROOF;
            if (w.storage_second[kind] >= (proof ? 8U : 64U) ||
                w.storage[kind] >= (proof ? 120U : 2048U) ||
                bytes > (32ULL << 20) - w.storage_bytes_second ||
                bytes > (512ULL << 20) - w.storage_bytes) return false;
            ++w.storage_second[kind];
            ++w.storage[kind];
            w.storage_bytes_second += bytes;
            w.storage_bytes += bytes;
            return true;
        }
        if (w.operations_second >= 8 || w.operations >= 120 || bytes > (1U << 20) - w.bytes_second) return false;
        ++w.operations_second;
        ++w.operations;
        w.bytes_second += bytes;
        return true;
    }

private:
    struct Window {
        /// \brief Начало текущего минутного окна для данного IP.
        std::chrono::steady_clock::time_point start;
        /// \brief Последняя обработанная секунда относительно `start`.
        int64_t second{0};
        /// \brief Число допущенных кандидат-операций за минутное окно.
        size_t operations{0};
        /// \brief Число допущенных кандидат-операций за текущую секунду.
        size_t operations_second{0};
        /// \brief Число новых подключений за минутное окно.
        size_t connections{0};
        /// \brief Число новых подключений за текущую секунду.
        size_t connections_second{0};
        /// \brief Byte-rate операций за текущую секунду.
        size_t bytes_second{0};
        /// \brief Счетчики по `STORAGE_PUT`, `STORAGE_GET`, `STORAGE_PROOF` за минуту и за секунду.
        std::array<size_t, 3> storage{}, storage_second{};
        /// \brief Совокупный storage byte-rate за минуту и за секунду.
        size_t storage_bytes{0}, storage_bytes_second{0};
    };

    /// \brief Защищает `m_last_cleanup` и `m_peers`.
    std::mutex m_mutex;
    /// \brief Раздельное состояние конкурентных storage-передач, живущее дольше временных lease.
    std::shared_ptr<Transfers> m_transfers{std::make_shared<Transfers>()};
    /// \brief Последний момент глобальной очистки истекших окон.
    std::chrono::steady_clock::time_point m_last_cleanup{};
    /// \brief Активные минутные окна по числовому IP-адресу; локальный лимит памяти — `4096` записей.
    std::map<std::string, Window> m_peers;
};
}
#endif
