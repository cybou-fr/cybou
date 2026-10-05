// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Публично-безопасная запись node events в локальный журнал.
#ifndef CYBOU_EVENT_RECORD_H
#define CYBOU_EVENT_RECORD_H
#include <cybou/diagnostics.h>
#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <map>
#include <optional>
#include <mutex>
#include <string>
#include <variant>
namespace cybou {
/// \brief Набор allowlisted событий локального node event log.
enum class NodeEvent {
    node_started,          ///< Локальный запуск Full Node.
    node_stopping,         ///< Контролируемая остановка узла.
    node_status,           ///< Снимок публично-безопасного статуса узла.
    peer_connected,        ///< Подключение peer'а.
    peer_disconnected,     ///< Отключение peer'а.
    peer_rejected,         ///< Отвергнутое подключение peer'а.
    sync_started,          ///< Начало локальной синхронизации.
    sync_progress,         ///< Промежуточный прогресс синхронизации.
    sync_complete,         ///< Синхронизация завершена.
    sync_failed,           ///< Синхронизация завершилась ошибкой.
    operation_received,    ///< Получена кандидат-операция.
    operation_accepted,    ///< Кандидат-операция принята в локальный volatile pool.
    operation_rejected,    ///< Кандидат-операция отвергнута.
    operation_uncertain,   ///< Локальное состояние операции неопределённо.
    operation_finalized,   ///< Операция вошла в финализированный блок.
    block_produced,        ///< Локально собран блок для PoA.
    block_received,        ///< Получен блок от сети.
    block_verified,        ///< Блок локально верифицирован.
    block_finalized,       ///< Блок стал канонически финализированным.
    poa_safety_halt,       ///< Локальный PoA safety guard сработал fail-closed.
    storage_connected,     ///< Наблюдается peer с доступным storage identity.
    storage_disconnected,  ///< Storage peer перестал наблюдаться.
    chunk_put,             ///< Локальная запись chunk-а.
    chunk_get,             ///< Локальное чтение chunk-а.
    chunk_verify_failed,   ///< Проверка chunk-а не прошла.
    placement_created,     ///< Создана новая remote placement.
    placement_degraded,    ///< Placement деградировал.
    placement_repaired,    ///< Placement восстановлен.
    content_securing,      ///< Идёт достижение durability.
    content_protected,     ///< Durability подтверждена.
    storage_audit_started, ///< Запущен storage audit.
    storage_audit_failed,  ///< Storage audit выявил проблему.
    storage_audit_repaired, ///< Последствия storage audit устранены.
    block_production_retry, ///< Повторная попытка локального block production.
};
using EventValue = std::variant<std::string, std::uint64_t, bool>;
using EventFields = std::map<std::string, EventValue>;
/// \brief Режим детализации event log без изменения allowlist-политики.
enum class EventLogMode { MINIMAL, DETAILED };
/// \brief Потокобезопасный writer локального event log с жёсткой allowlist полей.
class EventWriter final {
    mutable std::mutex m_mutex;
    std::FILE* m_file{nullptr};
    EventLogMode m_mode;
    std::string m_run;
    std::uint64_t m_sequence{0};
    std::mutex m_snapshot_mutex;
    std::map<std::string, PeerDiagnostics> m_peers;
    std::optional<EventFields> m_last_status; ///< Последний записанный node_status.
    bool m_last_halted{false};
public:
    /// \brief Открывает event log и генерирует новый run id.
    /// \throws std::runtime_error, если файл нельзя безопасно открыть или невозможно получить случайный `run_id`.
    explicit EventWriter(const std::filesystem::path& path, EventLogMode mode=EventLogMode::MINIMAL);
    ~EventWriter();
    /// \brief Записывает одно событие с allowlisted публичными полями.
    /// \throws std::invalid_argument при неразрешённых полях/слишком длинных строках; `std::runtime_error` при ошибке записи.
    void Write(NodeEvent event, const EventFields& fields = {});
    /// \brief Возвращает пригодность файлового дескриптора для дальнейшей записи.
    bool Good() const;
    /// \brief Проецирует диагностический snapshot в последовательность публичных событий.
    void Observe(const NodeDiagnosticsSnapshot& snapshot);
};
} // namespace cybou
#endif
