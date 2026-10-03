// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Синхронный workflow claim'а имени .cybou поверх node runtime и keystore.

#ifndef CYBOU_NAME_SERVICE_H
#define CYBOU_NAME_SERVICE_H

#include <cybou/keystore.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/node_runtime.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

namespace cybou {

/// \brief Фазы синхронного claim workflow для имени .cybou.
enum class NameClaimPhase { SAVING, COMMITTING, WAITING_FOR_COMMIT, WORKING, REVEALING, WAITING_FOR_NAME, ACTIVE, FAILED };
/// \brief Итоговая информация о выполнении claim workflow.
struct NameClaimResult {
    bool success{false};
    NameClaimPhase phase{NameClaimPhase::FAILED};
    std::string message;
};
using NamePhaseCallback = std::function<void(NameClaimPhase, const std::string&)>;

/// \brief Высокоуровневый orchestrator commit/reveal workflow для назначения имени текущему аккаунту.
class CybouNameService {
public:
    /// \brief Создаёт name service поверх runtime, keystore и пути к identity vault.
    CybouNameService(CybouNodeRuntime& runtime, CybouKeyStore& keystore, std::filesystem::path identity_vault_path);
    /// \brief Выполняет полный синхронный claim имени: persist secret, commit, ожидание, reveal и финализацию.
    NameClaimResult ClaimSync(std::string label, std::string password,
        const NamePhaseCallback& on_phase = {},
        std::chrono::milliseconds timeout = std::chrono::seconds(60));
    /// \brief Просит текущий ClaimSync завершиться как можно скорее.
    void Cancel() { m_cancelled.store(true); }

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    IdentityOperationCoordinator& m_operation_coordinator;
    std::filesystem::path m_identity_vault_path;
    std::filesystem::path m_claim_path;
    std::atomic<bool> m_cancelled{false};
};

} // namespace cybou
#endif
