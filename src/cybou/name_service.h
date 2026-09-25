// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_NAME_SERVICE_H
#define CYBOU_NAME_SERVICE_H

#include <cybou/keystore.h>
#include <cybou/node_runtime.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <string>

namespace cybou {

enum class NameClaimPhase { SAVING, COMMITTING, WAITING_FOR_COMMIT, WORKING, REVEALING, WAITING_FOR_NAME, ACTIVE, FAILED };
struct NameClaimResult {
    bool success{false};
    NameClaimPhase phase{NameClaimPhase::FAILED};
    std::string message;
};
using NamePhaseCallback = std::function<void(NameClaimPhase, const std::string&)>;

class CybouNameService {
public:
    CybouNameService(CybouNodeRuntime& runtime, CybouKeyStore& keystore, std::filesystem::path identity_vault_path);
    NameClaimResult ClaimSync(std::string label, std::string password,
        const NamePhaseCallback& on_phase = {},
        std::chrono::milliseconds timeout = std::chrono::seconds(60));
    void Cancel() { m_cancelled.store(true); }

private:
    CybouNodeRuntime& m_runtime;
    CybouKeyStore& m_keystore;
    std::filesystem::path m_identity_vault_path;
    std::filesystem::path m_claim_path;
    std::atomic<bool> m_cancelled{false};
};

} // namespace cybou
#endif
