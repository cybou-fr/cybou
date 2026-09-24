// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/identity_service.h>

#include <random.h>

namespace cybou {

CybouIdentityService::CybouIdentityService(CybouNodeRuntime& runtime)
    : m_runtime{runtime}
{
}

CybouIdentityService::~CybouIdentityService()
{
    Cancel();
    if (m_worker.joinable()) {
        m_worker.join();
    }
}

std::optional<AccountId> CybouIdentityService::GetAccountId() const
{
    std::lock_guard lock(m_mutex);
    return m_keystore.GetAccountId();
}

bool CybouIdentityService::LoadExistingIdentity(const std::array<unsigned char, 32>& priv_key_seed)
{
    std::lock_guard lock(m_mutex);
    if (!m_keystore.LoadFromSeed(priv_key_seed)) return false;
    const auto acc_id = m_keystore.GetAccountId();
    if (!acc_id) return false;

    const auto account_state = m_runtime.GetAccountState(*acc_id);
    if (account_state.has_value()) {
        m_phase.store(IdentityCreationPhase::ACTIVE);
    } else {
        m_phase.store(IdentityCreationPhase::IDLE);
    }
    return true;
}

bool CybouIdentityService::LoadKeyStore(const std::filesystem::path& path)
{
    std::lock_guard lock(m_mutex);
    if (!m_keystore.LoadFromFile(path)) return false;
    const auto acc_id = m_keystore.GetAccountId();
    if (!acc_id) return false;

    const auto account_state = m_runtime.GetAccountState(*acc_id);
    if (account_state.has_value()) {
        m_phase.store(IdentityCreationPhase::ACTIVE);
    } else {
        m_phase.store(IdentityCreationPhase::IDLE);
    }
    return true;
}

bool CybouIdentityService::SaveKeyStore(const std::filesystem::path& path) const
{
    std::lock_guard lock(m_mutex);
    return m_keystore.SaveToFile(path);
}

namespace {

IdentityCreationResult Failure(
    IdentityCreationPhase phase,
    std::string error_message,
    const AccountId& account_id = {})
{
    return IdentityCreationResult{
        .success = false,
        .final_phase = phase,
        .account_id = account_id,
        .creation_height = 0,
        .system_balance = 0,
        .error_message = std::move(error_message),
    };
}

} // namespace

IdentityCreationResult CybouIdentityService::CreateIdentitySync(
    const PhaseCallback& on_phase,
    const std::optional<std::array<unsigned char, 32>>& user_provided_key,
    const std::chrono::milliseconds timeout)
{
    m_cancelled.store(false);

    // Phase 1: CREATING_KEYS
    m_phase.store(IdentityCreationPhase::CREATING_KEYS);
    if (on_phase) on_phase(IdentityCreationPhase::CREATING_KEYS, "Generating local keys...");

    AccountId account_id;
    AccountAuthorizationV1 auth;
    {
        std::lock_guard lock(m_mutex);
        if (user_provided_key.has_value()) {
            if (!m_keystore.LoadFromSeed(*user_provided_key)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Failed to load user-provided key");
            }
        } else if (!m_keystore.HasKey()) {
            if (!m_keystore.GenerateNew()) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Failed to generate local key");
            }
        }
        const auto pubkey = m_keystore.GetPublicKey();
        const auto acc_opt = m_keystore.GetAccountId();
        if (!pubkey || !acc_opt) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Invalid identity key");
        }
        account_id = *acc_opt;
        auth.authorization_descriptor = *pubkey;
    }

    // Check if account already exists in state
    const auto existing = m_runtime.GetAccountState(account_id);
    if (existing.has_value()) {
        m_phase.store(IdentityCreationPhase::ACTIVE);
        if (on_phase) on_phase(IdentityCreationPhase::ACTIVE, "Identity active.");
        return IdentityCreationResult{
            .success = true,
            .final_phase = IdentityCreationPhase::ACTIVE,
            .account_id = account_id,
            .creation_height = existing->creation_height,
            .system_balance = existing->system_balance,
            .error_message = {},
        };
    }

    if (m_cancelled.load()) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cancelled", account_id);
    }

    // Phase 2: PERFORMING_WORK
    m_phase.store(IdentityCreationPhase::PERFORMING_WORK);
    if (on_phase) on_phase(IdentityCreationPhase::PERFORMING_WORK, "Computing anti-Sybil proof-of-work...");

    const uint64_t height = m_runtime.GetFinalizedHeight().value_or(0);
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    const uint64_t current_epoch = EpochForHeight(height, params);

    AccountCreationWorkV1 work{
        .version = ACCOUNT_CREATION_WORK_VERSION,
        .network_id = m_runtime.GetNetworkId(),
        .account_id = account_id,
        .initial_authorization_commitment = ComputeAuthCommitment(auth),
        .work_epoch = current_epoch,
        .nonce = 0,
    };

    while (!m_cancelled.load() && !CheckAccountCreationWork(work, params.account_creation_work_bits)) {
        ++work.nonce;
    }

    if (m_cancelled.load()) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cancelled", account_id);
    }

    // Phase 3: BROADCASTING
    m_phase.store(IdentityCreationPhase::BROADCASTING);
    if (on_phase) on_phase(IdentityCreationPhase::BROADCASTING, "Signing and submitting AccountCreateOp...");

    const uint256 pop_digest = ComputeAccountPopDigest(m_runtime.GetNetworkId(), account_id, auth.authorization_descriptor);
    std::optional<std::array<unsigned char, 64>> pop_sig;
    {
        std::lock_guard lock(m_mutex);
        pop_sig = m_keystore.Sign(pop_digest);
    }
    if (!pop_sig.has_value()) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to sign proof of possession", account_id);
    }

    AccountCreateOpV1 op{
        .version = ACCOUNT_CREATE_OP_VERSION,
        .account_id = account_id,
        .initial_authorization = auth,
        .creation_work = work,
        .proof_of_possession = *pop_sig,
    };

    if (!m_runtime.SubmitOperation(ProtocolOperationV1{op})) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to submit AccountCreateOp to node runtime", account_id);
    }

    // Phase 4: WAITING_FOR_FINALITY
    m_phase.store(IdentityCreationPhase::WAITING_FOR_FINALITY);
    if (on_phase) on_phase(IdentityCreationPhase::WAITING_FOR_FINALITY, "Waiting for BFT finality certificate...");

    if (m_runtime.GetStatus().is_authority) {
        m_runtime.ProduceBlock();
    }

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        const auto state = m_runtime.GetAccountState(account_id);
        if (state.has_value()) {
            m_phase.store(IdentityCreationPhase::ACTIVE);
            if (on_phase) on_phase(IdentityCreationPhase::ACTIVE, "Identity active.");
            return IdentityCreationResult{
                .success = true,
                .final_phase = IdentityCreationPhase::ACTIVE,
                .account_id = account_id,
                .creation_height = state->creation_height,
                .system_balance = state->system_balance,
                .error_message = {},
            };
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    m_phase.store(IdentityCreationPhase::FAILED);
    return Failure(IdentityCreationPhase::FAILED, m_cancelled.load() ? "Cancelled" : "Timed out waiting for BFT finality", account_id);
}

void CybouIdentityService::CreateIdentityAsync(
    PhaseCallback on_phase,
    CompletionCallback on_complete,
    const std::optional<std::array<unsigned char, 32>>& user_provided_key,
    const std::chrono::milliseconds timeout)
{
    Cancel();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    m_cancelled.store(false);
    m_worker = std::jthread([this, on_phase = std::move(on_phase), on_complete = std::move(on_complete), user_provided_key, timeout]() {
        const auto result = CreateIdentitySync(on_phase, user_provided_key, timeout);
        if (on_complete) {
            on_complete(result);
        }
    });
}

void CybouIdentityService::Cancel()
{
    m_cancelled.store(true);
}

} // namespace cybou
