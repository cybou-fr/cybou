// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

/// \file
/// Координатор точной сериализации, журналирования и повторной отправки операций Identity.

#ifndef CYBOU_IDENTITY_OPERATION_COORDINATOR_H
#define CYBOU_IDENTITY_OPERATION_COORDINATOR_H

#include <cybou/identity_registry.h>
#include <cybou/protocol_operation.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>

namespace cybou {

class CybouKeyStore;
class CybouNodeRuntime;

/// Текущее состояние локально координируемой операции Identity.
enum class IdentityOperationPhase : uint8_t {
    /// Точные байты ещё не отправлялись в сеть.
    PREPARED,
    /// Идёт первичная отправка exact bytes в Full Node runtime.
    SUBMITTING,
    /// Доставка в сеть не подтверждена и не опровергнута; replacement запрещён.
    UNCERTAIN,
    /// Full Node принял кандидат-операцию локально, но финализация ещё не наступила.
    ACCEPTED,
    /// Кандидат-операция вошла в PoA-финализированный блок.
    FINALIZED,
    /// Локальная проверка или сетевой приём отвергли exact bytes.
    REJECTED,
    /// Финализирован конфликтующий результат с тем же nonce/key epoch.
    CONFLICT,
};

/// Результат локального выполнения или согласования одной операции Identity.
struct IdentityOperationResult {
    /// Последняя известная фаза жизненного цикла exact bytes.
    IdentityOperationPhase phase{IdentityOperationPhase::REJECTED};
    /// OperationID канонических exact bytes; может быть нулевым при ранней ошибке.
    cybou::Hash256 op_id;
    /// Высота блока финализации или `0`, если финализация ещё не наблюдалась.
    uint64_t finalized_height{0};
    /// Диагностическое описание локальной/сетевой ошибки; не должно содержать секретов.
    std::string error;
    explicit operator bool() const
    {
        return phase == IdentityOperationPhase::ACCEPTED || phase == IdentityOperationPhase::UNCERTAIN ||
            phase == IdentityOperationPhase::FINALIZED;
    }
};

/// Фабрика канонической операции для уже подготовленной авторизации.
using IdentityOperationBuilder = std::function<std::optional<ProtocolOperation>(const IdentityOperationAuthorization&)>;

/// Сериализует операции одной Identity по nonce и журналирует точные байты.
class IdentityOperationCoordinator {
public:
    /// Создаёт координатор с журналом и интервалом повторной ретрансляции.
    /// \param runtime Текущий Full Node runtime.
    /// \param keystore Разблокированный локальный keystore Identity.
    /// \param journal_path Путь durable-журнала exact bytes.
    /// \param relay_retry_interval Интервал между повторными relay для фазы `UNCERTAIN`.
    /// \pre `relay_retry_interval > 0`.
    /// \post Координатор не начинает сетевую работу до явных вызовов `Execute`/`RotateIdentity`/`RetryRelayIfDue`.
    /// \thread_safety Экземпляр потокобезопасен для конкурентных вызовов публичных методов; внутри использует `m_mutex`.
    IdentityOperationCoordinator(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
        std::filesystem::path journal_path,
        std::chrono::milliseconds relay_retry_interval = std::chrono::seconds{30});
    ~IdentityOperationCoordinator();
    IdentityOperationCoordinator(const IdentityOperationCoordinator&) = delete;
    IdentityOperationCoordinator& operator=(const IdentityOperationCoordinator&) = delete;

    /// Строит, подписывает, журналирует и отправляет обычную операцию Identity.
    /// \param kind Вид пользовательской кандидат-операции.
    /// \param payload_commitment Коммитмент payload, уже вычисленный вызывающим кодом.
    /// \param build Фабрика `ProtocolOperation`, обязанная использовать переданную authorization как есть.
    /// \return Фаза и `OperationID`; при ошибке возвращает `REJECTED`/`UNCERTAIN` с заполненным `error`.
    /// \pre В keystore разблокирована Identity, а `build` не создаёт replacement с другим nonce.
    /// \post При успешной локальной подготовке exact bytes сохраняются в durable-журнал до разрешения исхода.
    IdentityOperationResult Execute(IdentityOperationKind kind, const IdentityKeyId& payload_commitment,
        const IdentityOperationBuilder& build);
    /// Выполняет атомарную ротацию всех публичных возможностей Identity.
    /// \param new_identity_entropy Новый recovery entropy; секрет не логировать и очистить у вызывающего кода.
    /// \return Результат отправки/согласования IdentityRotate.
    /// \pre Keystore разблокирован и отражает текущую финализированную Identity.
    /// \post Пока исход ротации `UNCERTAIN`, новый vault нельзя продвигать поверх активного.
    IdentityOperationResult RotateIdentity(std::span<const unsigned char, 32> new_identity_entropy);
    /// Повторяет отправку неопределённой операции, если наступил её срок.
    /// \return `true`, если повтор выполнен без внутренней ошибки или не требовался; `false` при ошибке чтения/журнала.
    /// \post Replacement exact bytes не создаются: ретранслируется только уже зажурналированный payload.
    bool RetryRelayIfDue();
    /// Завершает локальную фазу ротации после её финализации в каноническом состоянии.
    /// \param finalized_identity Финализированная запись Identity из состояния.
    /// \return `true`, если локальный журнал/кандидат vault согласованы с финализированным состоянием.
    /// \pre Вызывается только после наблюдения PoA-финализации соответствующей ротации.
    /// \post При успехе незавершённая локальная ротация больше не считается pending.
    bool CompleteIdentityRotation(const IdentityRecord& finalized_identity);
    /// Возвращает true, если локально есть незавершённая ротация Identity.
    /// \return `true`, если журнал содержит unresolved IdentityRotate.
    bool HasPendingIdentityRotation();
    /// Запрашивает актуальный статус операции по её идентификатору.
    /// \param op_id Идентификатор exact bytes операции.
    /// \return Текущий известный статус; `error` содержит причину локальной невозможности проверить состояние.
    /// \post Метод не модифицирует журнал, кроме возможной ленивой загрузки.
    IdentityOperationResult GetStatus(const cybou::Hash256& op_id);

private:
    struct JournalEntry;
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    std::filesystem::path m_journal_path;
    const std::chrono::milliseconds m_relay_retry_interval;
    mutable std::mutex m_mutex;
    std::optional<std::chrono::steady_clock::time_point> m_next_relay_retry;
    std::unique_ptr<JournalEntry> m_entry;
    bool m_loaded{false};
    std::string m_load_error;

    bool LoadJournal();
    bool SaveJournal(const JournalEntry& entry);
    bool ClearJournal();
    IdentityOperationResult SubmitExact(JournalEntry& entry);
    IdentityOperationResult Reconcile(JournalEntry& entry);
};

} // namespace cybou

#endif // CYBOU_IDENTITY_OPERATION_COORDINATOR_H
