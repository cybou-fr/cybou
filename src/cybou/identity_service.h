// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_IDENTITY_SERVICE_H
#define CYBOU_IDENTITY_SERVICE_H

#include <cybou/account_creation.h>
#include <cybou/account_id.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/signing.h>
#include <support/cleanse.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace cybou {

enum class IdentityCreationPhase : uint8_t {
    IDLE = 0,
    CREATING_KEYS,
    PERFORMING_WORK,
    BROADCASTING,
    WAITING_FOR_FINALITY,
    ACTIVE,
    FAILED,
};

struct IdentityCreationResult {
    bool success{false};
    IdentityCreationPhase final_phase{IdentityCreationPhase::IDLE};
    AccountId account_id;
    uint64_t creation_height{0};
    uint64_t system_balance{0};
    std::string error_message;
};

using PhaseCallback = std::function<void(IdentityCreationPhase phase, const std::string& detail)>;
using CompletionCallback = std::function<void(const IdentityCreationResult& result)>;

class CybouIdentityService {
public:
    explicit CybouIdentityService(
        CybouNodeRuntime& runtime,
        std::optional<std::filesystem::path> storage_path = std::nullopt);
    ~CybouIdentityService();

    CybouIdentityService(const CybouIdentityService&) = delete;
    CybouIdentityService& operator=(const CybouIdentityService&) = delete;

    /** Configure persistent storage path for atomic pre-save */
    void SetStoragePath(std::filesystem::path path);
    std::optional<std::filesystem::path> GetStoragePath() const;

    /** Current identity creation phase */
    IdentityCreationPhase GetPhase() const { return m_phase.load(); }

    /** Account ID if an identity has been initialized or created */
    std::optional<AccountId> GetAccountId() const;

    /** Load an existing identity from raw private key seed (cleansed after loading) */
    bool LoadExistingIdentity(const std::array<unsigned char, 32>& priv_key_seed);

    /** Load / save protected key from / to file */
    bool LoadKeyStore(const std::filesystem::path& path);
    bool SaveKeyStore(const std::filesystem::path& path) const;

    /** Access underlying keystore */
    CybouKeyStore& GetKeyStore() { return m_keystore; }
    const CybouKeyStore& GetKeyStore() const { return m_keystore; }

    /** Synchronous identity creation (blocks until complete or error) */
    IdentityCreationResult CreateIdentitySync(
        const PhaseCallback& on_phase = nullptr,
        const std::optional<std::array<unsigned char, 32>>& user_provided_key = std::nullopt,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /** Asynchronous identity creation */
    void CreateIdentityAsync(
        PhaseCallback on_phase,
        CompletionCallback on_complete,
        const std::optional<std::array<unsigned char, 32>>& user_provided_key = std::nullopt,
        std::chrono::milliseconds timeout = std::chrono::seconds(30));

    /** Cancel ongoing identity creation */
    void Cancel();

private:
    CybouNodeRuntime& m_runtime;
    std::atomic<IdentityCreationPhase> m_phase{IdentityCreationPhase::IDLE};
    std::atomic<bool> m_cancelled{false};
    std::optional<std::filesystem::path> m_storage_path;
    CybouKeyStore m_keystore;
    mutable std::mutex m_mutex;
    std::jthread m_worker;
};

} // namespace cybou

#endif // CYBOU_IDENTITY_SERVICE_H
