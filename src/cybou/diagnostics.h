// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
/// \file
/// \brief Русские описания структур диагностического снимка узла.
#ifndef CYBOU_DIAGNOSTICS_H
#define CYBOU_DIAGNOSTICS_H
#include <cstdint>
#include <string>
#include <vector>
namespace cybou {

/// \brief Диагностические сведения об одном подключенном пире.
struct PeerDiagnostics {
    std::string endpoint;
    std::uint64_t advertised_height{0};
    std::string storage_id;
};

/// \brief Диагностические сведения об одной операции во внутренних очередях.
struct OperationDiagnostics {
    std::string operation_id;
    std::uint32_t state{0};
    std::uint64_t finalized_height{0};
};

/// \brief Полный снимок локального состояния узла для CLI и UI-диагностики.
/// \details Каноническая вершина только одна; высоты пиров здесь остаются недоверенными объявлениями.
struct NodeDiagnosticsSnapshot {
    std::string network_binding, node_type, tip, state_root;
    std::uint64_t height{0}, storage_used{0}, storage_capacity{0};
    bool initialized{false}, safety_halted{false}, poa_signer_active{false}, validation_eligible{false};
    std::vector<PeerDiagnostics> peers;
    std::vector<OperationDiagnostics> operations;
};
} // namespace cybou
#endif
