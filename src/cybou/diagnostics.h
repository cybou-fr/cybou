// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Русские описания структур диагностического снимка узла.
#ifndef CYBOU_DIAGNOSTICS_H
#define CYBOU_DIAGNOSTICS_H
#include <cstdint>
#include <optional>
#include <memory>
#include <cybou/traffic_meter.h>
#include <cybou/finalization_meter.h>
#include <cybou/process_cpu.h>
#include <string>
#include <vector>
namespace cybou {
struct NetworkObservationSnapshot;

/// \brief Диагностические сведения об одном подключенном пире.
struct PeerDiagnostics {
    /// \brief Числовой `IP:port`, по которому установлена текущая P2P-сессия.
    std::string endpoint;
    /// \brief Финализованная высота, заявленная пиром в последнем HELLO или ответе.
    /// \details Это только удаленное объявление liveness/маршрутизации, а не локально подтвержденная каноническая высота.
    std::uint64_t advertised_height{0};
    /// \brief Доказанный `StorageId` удаленного Full Node, если для этой сессии уже был выполнен on-demand `STORAGE_PROOF`.
    std::string storage_id;
};

/// \brief Диагностические сведения об одной операции во внутренних очередях.
struct OperationDiagnostics {
    /// \brief Канонический `OperationID` в прямом hex-порядке байт.
    std::string operation_id;
    /// \brief Внутренний код состояния локальной обработки/ретрансляции.
    std::uint32_t state{0};
    /// \brief Финализованная высота базы, относительно которой кандидат-операция сейчас отслеживается локально.
    std::uint64_t finalized_height{0};
};

/// \brief Полный снимок локального состояния узла для CLI и UI-диагностики.
/// \details Каноническая вершина только одна; высоты пиров здесь остаются недоверенными объявлениями.
struct NodeDiagnosticsSnapshot {
    std::shared_ptr<const NetworkObservationSnapshot> network_observation;
    TrafficDiagnostics traffic;
    FinalizationDiagnostics finalization;
    ProcessCpuDiagnostics process_cpu;
    /// OS working set / RSS for the whole local executable, not host or network memory.
    std::optional<std::uint64_t> process_resident_bytes;
    /// Local passive observation time (UTC) and monotonic runtime lifetime.
    /// A zero observation time means no real runtime sample is available.
    std::uint64_t observed_unix_ms{0}, uptime_ms{0};
    /// Actual volatile candidate pool, distinct from recent operation history.
    std::uint64_t pending_operations{0}, pending_operation_bytes{0};
    /// \brief `NetworkBinding` активной официальной сети в hex.
    /// \brief Локальный тип узла для UI/CLI; в текущем базовом варианте это обычный `Full Node`.
    /// \brief `BlockID` текущей локально финализованной вершины в hex.
    /// \brief `state_root` текущего локально финализованного состояния в hex.
    std::string network_binding, node_type, tip, state_root;
    /// \brief Высота локально финализованной вершины.
    /// \brief Текущий объем занятых локальных storage-байт.
    /// \brief Локальная политика емкости storage в байтах.
    std::uint64_t height{0}, storage_used{0}, storage_capacity{0};
    /// rief Занятые bytes всего локального ChunkBlobStore (own, cache и provider).
    /// rief Локальная CYBOU capacity `V`; `storage_capacity` выше — provider budget `floor(2V/3)`.
    std::uint64_t local_storage_used{0}, local_storage_capacity{0};
    /// Available filesystem bytes, distinct from V and provider budget. No reservation promise.
    std::optional<std::uint64_t> storage_disk_available;
    /// \brief `true`, когда runtime инициализирован и может отвечать непротиворечивым состоянием.
    /// \brief `true`, когда локальная защитная логика остановила небезопасный путь fail-closed.
    /// \brief `true`, когда локальный PoA signer сейчас активен и не отключен правилами signing safety.
    bool initialized{false}, safety_halted{false}, poa_signer_active{false};
    /// \brief Снимок всех подключенных P2P-пиров на момент формирования структуры.
    std::vector<PeerDiagnostics> peers;
    /// \brief Снимок операций, которые локальный runtime еще отслеживает вне уже финализованной истории.
    std::vector<OperationDiagnostics> operations;
};
} // namespace cybou
#endif
