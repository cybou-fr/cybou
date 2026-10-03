// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
#ifndef CYBOU_DIAGNOSTICS_H
#define CYBOU_DIAGNOSTICS_H
#include <cstdint>
#include <string>
#include <vector>
namespace cybou {
struct PeerDiagnostics {
    std::string endpoint;
    std::uint64_t advertised_height{0};
    std::uint64_t capabilities{0};
    std::string provider_id;
};
struct OperationDiagnostics {
    std::string operation_id;
    std::uint32_t state{0};
    std::uint64_t finalized_height{0};
};
/** One immutable canonical head; peer heights are untrusted advertisements. */
struct NodeDiagnosticsSnapshot {
    std::string network_binding, role, tip, state_root;
    std::uint64_t height{0}, storage_used{0}, storage_capacity{0};
    bool initialized{false}, safety_halted{false};
    std::vector<PeerDiagnostics> peers;
    std::vector<OperationDiagnostics> operations;
};
} // namespace cybou
#endif
