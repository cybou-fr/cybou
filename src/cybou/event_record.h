// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
/// \file
/// \brief Публично-безопасная запись node events в локальный журнал.
#ifndef CYBOU_EVENT_RECORD_H
#define CYBOU_EVENT_RECORD_H
#include <cybou/diagnostics.h>
#include <cstdint>
#include <filesystem>
#include <cstdio>
#include <map>
#include <mutex>
#include <string>
#include <variant>
namespace cybou {
/// \brief Набор allowlisted событий локального node event log.
enum class NodeEvent {
    node_started, node_stopping, node_status, peer_connected, peer_disconnected,
    peer_rejected, sync_started, sync_progress, sync_complete, sync_failed,
    operation_received, operation_accepted, operation_rejected, operation_uncertain,
    operation_finalized, block_produced, block_received, block_verified,
    block_finalized, poa_safety_halt, storage_connected, storage_disconnected,
    chunk_put, chunk_get, chunk_verify_failed, placement_created, placement_degraded,
    placement_repaired, content_securing, content_protected, storage_audit_started,
    storage_audit_failed, storage_audit_repaired, block_production_retry,
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
public:
    /// \brief Открывает event log и генерирует новый run id.
    explicit EventWriter(const std::filesystem::path& path, EventLogMode mode=EventLogMode::MINIMAL);
    ~EventWriter();
    /// \brief Записывает одно событие с allowlisted публичными полями.
    void Write(NodeEvent event, const EventFields& fields = {});
    /// \brief Возвращает пригодность файлового дескриптора для дальнейшей записи.
    bool Good() const;
    /// \brief Проецирует диагностический snapshot в последовательность публичных событий.
    void Observe(const NodeDiagnosticsSnapshot& snapshot);
};
} // namespace cybou
#endif
