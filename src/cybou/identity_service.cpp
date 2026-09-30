// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/identity_service.h>
#include <cybou/block_executor.h>
#include <cybou/account_creation.h>

#include <string>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/identity_vault.h>
#include <cybou/crypto/cleanse.h>

#include <algorithm>

namespace cybou {
namespace {
bool IsAuthorizedIdentity(const CybouNodeRuntime& runtime, const AccountId& account_id, const CybouKeyStore& keystore);
}

CybouIdentityService::CybouIdentityService(
    CybouNodeRuntime& runtime,
    std::optional<std::filesystem::path> storage_path)
    : m_runtime{runtime}, m_storage_path{std::move(storage_path)}
{
}

void CybouIdentityService::SetStoragePath(std::filesystem::path path)
{
    std::lock_guard lock(m_mutex);
    if (m_storage_path != path) m_vault_saved = false;
    m_storage_path = std::move(path);
}

std::optional<std::filesystem::path> CybouIdentityService::GetStoragePath() const
{
    std::lock_guard lock(m_mutex);
    return m_storage_path;
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

std::optional<AccountState> CybouIdentityService::GetFinalizedAccountState() const
{
    const auto account_id = GetAccountId();
    return account_id ? m_runtime.GetAccountState(*account_id) : std::nullopt;
}

std::optional<std::string> CybouIdentityService::GetFinalizedPrimaryName() const
{
    const auto account_id = GetAccountId();
    if (!account_id) return std::nullopt;
    const auto loaded = m_runtime.GetStore().LoadState();
    if (!loaded || !loaded.state) return std::nullopt;
    const auto* name = loaded.state->names.PrimaryName(*account_id);
    return name ? std::optional<std::string>{*name} : std::nullopt;
}

std::optional<RecoveryWords> CybouIdentityService::PrepareNewIdentity()
{
    std::lock_guard lock(m_mutex);
    if (!m_storage_path || std::filesystem::exists(*m_storage_path) || m_vault_saved) return std::nullopt;
    if (m_keystore.HasKey()) return m_keystore.GetRecoveryWords();
    if (!m_keystore.GenerateNew()) return std::nullopt;
    return m_keystore.GetRecoveryWords();
}

void CybouIdentityService::DiscardPreparedIdentity()
{
    std::lock_guard lock(m_mutex);
    if (!m_vault_saved && (m_phase.load() == IdentityCreationPhase::IDLE ||
        m_phase.load() == IdentityCreationPhase::FAILED)) m_keystore.Clear();
}

bool CybouIdentityService::IsNetworkAuthority() const
{
    std::lock_guard lock(m_mutex);
    return m_keystore.DerivesPublicKey(IdentityKeyPurpose::POA_FINALIZER,
        m_runtime.GetNetworkDefinition().poa_finalizer_public_key);
}

bool CybouIdentityService::LoadVault(std::string_view password)
{
    std::lock_guard lock(m_mutex);
    if (!m_storage_path || !m_keystore.LoadFromFile(*m_storage_path, password)) return false;
    m_vault_saved = true;
    const auto acc_id = m_keystore.GetAccountId();
    if (!acc_id) return false;

    const auto account_state = m_runtime.GetAccountState(*acc_id);
    if (account_state && IsAuthorizedIdentity(m_runtime, *acc_id, m_keystore)) {
        m_phase.store(IdentityCreationPhase::ACTIVE);
    } else {
        m_phase.store(IdentityCreationPhase::IDLE);
    }
    return true;
}

namespace {

struct PasswordWiper {
    std::string& value;
    ~PasswordWiper() { if (!value.empty()) crypto::CleanseMemory(value.data(), value.size()); }
};

bool IsAuthorizedIdentity(const CybouNodeRuntime& runtime, const AccountId& account_id, const CybouKeyStore& keystore)
{
    const auto authorization_key = keystore.GetAuthorizationPublicKey();
    const auto recovery_key = keystore.GetRecoveryPublicKey();
    const auto kem_public = keystore.GetIdentityXWingPublicKey();
    const auto package = kem_public ? EncodeIdentityKemPackage(*kem_public) : std::nullopt;
    const auto loaded = runtime.GetStore().LoadState();
    if (!authorization_key || !recovery_key || !package || !keystore.ValidateIdentityXWingKeyPair() || !loaded || !loaded.state) return false;
    const auto* record = loaded.state->identities.Find(account_id);
    if (!record || record->authorization_key != *authorization_key || record->recovery_key != *recovery_key) return false;
    const auto network_id = runtime.GetNetworkId();
    const auto account_bytes = account_id.Value();
    const auto commitment = ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32},
        record->key_epoch, *package);
    return commitment && *commitment == record->kem_package_id;
}

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
    std::string password,
    const PhaseCallback& on_phase,
    const std::chrono::milliseconds timeout)
{
    PasswordWiper wipe_password{password};
    (void)timeout;
    m_cancelled.store(false);

    // Phase 1: CREATING_KEYS
    m_phase.store(IdentityCreationPhase::CREATING_KEYS);
    if (on_phase) on_phase(IdentityCreationPhase::CREATING_KEYS, "Generating local keys...");

    AccountId account_id;
    IdentityAuthorization auth;
    IdentityKemPackage kem_package{};
    {
        std::lock_guard lock(m_mutex);
        if (!m_keystore.HasKey() || !m_storage_path) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Prepare identity and configure its vault before creation");
        }
        const auto acc_opt = m_keystore.GetAccountId();
        const auto authorization_key = m_keystore.GetAuthorizationPublicKey();
        const auto root_key = m_keystore.GetRecoveryPublicKey();
        const auto kem_public = m_keystore.GetIdentityXWingPublicKey();
        const auto package = kem_public ? EncodeIdentityKemPackage(*kem_public) : std::nullopt;
        if (!acc_opt || !authorization_key || !root_key || !package || !m_keystore.ValidateIdentityXWingKeyPair()) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Invalid identity key");
        }
        account_id = *acc_opt;
        kem_package = *package;
        auth = IdentityAuthorization{
            .recovery_root = *root_key,
            .authorization_key = *authorization_key,
        };

        // A new account cannot be broadcast until the portable vault has been
        // durably published and authenticated by reopening it.
        if (!m_vault_saved) {
            if (password.size() < 12 || !m_keystore.SaveToFile(*m_storage_path, password)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Failed to save and verify portable identity vault");
            }
            m_vault_saved = true;
        }
    }

    // Check if account already exists in state
    const auto existing = m_runtime.GetAccountState(account_id);
    if (existing.has_value()) {
        if (!IsAuthorizedIdentity(m_runtime, account_id, m_keystore)) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Unlocked Identity keys do not match finalized state", account_id);
        }
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

    const uint256 network_id = m_runtime.GetNetworkId();
    const auto account_bytes = account_id.Value();
    const auto kem_package_id = ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32},
        0, kem_package);
    const auto auth_commitment = kem_package_id ?
        ComputeAccountCreateAuthorizationCommitment(auth, *kem_package_id) : std::nullopt;
    if (!auth_commitment) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to compute authorization commitment", account_id);
    }

    AccountCreationWork work{
        .network_id = m_runtime.GetNetworkId(),
        .account_id = account_id,
        .authorization_commitment = *auth_commitment,
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

    const auto pop_digest = ComputeAccountCreatePopDigest(network_id, account_id, auth, *kem_package_id);
    if (!pop_digest) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to compute proof of possession digest", account_id);
    }

    std::optional<IdentityHybridSignature> rec_pop;
    std::optional<IdentityHybridSignature> authorization_pop;
    {
        std::lock_guard lock(m_mutex);
        rec_pop = m_keystore.SignRecovery(*pop_digest);
        authorization_pop = m_keystore.SignAuthorization(*pop_digest);
    }
    if (!rec_pop || !authorization_pop) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to sign proof of possession", account_id);
    }

    AccountCreateOp op{
        .account_id = account_id,
        .authorization = auth,
        .kem_package = kem_package,
        .work = work,
        .recovery_pop = *rec_pop,
        .authorization_pop = *authorization_pop,
    };

    if (const auto submitted = m_runtime.SubmitOperation(ProtocolOperation{op}); !submitted) {
        m_phase.store(IdentityCreationPhase::FAILED);
        // Say why: the difference decides what the user should do next.
        std::string reason;
        if (submitted.delivery_uncertain) {
            reason = "The CYBOU network did not answer. Check your connection and try again.";
        } else {
            switch (submitted.status) {
            case OperationSubmitStatus::NETWORK_MISMATCH:
                reason = "This app and the network it reached are on different CYBOU networks.";
                break;
            case OperationSubmitStatus::INVALID_PAYLOAD:
                reason = "The network could not read the new Identity (invalid payload).";
                break;
            default: {
                // The finalizer does not say why; replay the exact operation
                // against this node's own verified finalized state.
                reason = "The network rejected the new Identity";
                const auto next_height = m_runtime.GetFinalizedHeight().value_or(0) + 1;
                const auto check = ValidateAccountCreateOp(op, network_id, next_height, params);
                const auto loaded = m_runtime.GetStore().LoadState();
                if (check != AccountCreateError::NONE) {
                    reason += " (operation check " + std::to_string(static_cast<int>(check)) + ")";
                } else if (loaded && loaded.state) {
                    const auto replay = ExecuteBlockOperations(*loaded.state, {ProtocolOperation{op}}, network_id,
                        next_height, params);
                    reason += replay ? ". This computer may still be catching up with the network: wait until CYBOU shows Synced and try again"
                        : " (block error " + std::to_string(static_cast<int>(replay.error)) + ", create error " +
                            std::to_string(static_cast<int>(replay.create_error)) + ")";
                }
                reason += ".";
                break;
            }
            }
        }
        return Failure(IdentityCreationPhase::FAILED, reason, account_id);
    }

    // Phase 4: WAITING_FOR_FINALITY
    m_phase.store(IdentityCreationPhase::WAITING_FOR_FINALITY);
    if (on_phase) on_phase(IdentityCreationPhase::WAITING_FOR_FINALITY, "Waiting for PoA finality certificate...");

    if (m_runtime.GetStatus().is_finalizer) {
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
    return Failure(IdentityCreationPhase::FAILED, m_cancelled.load() ? "Cancelled" : "Timed out waiting for PoA finality", account_id);
}

void CybouIdentityService::CreateIdentityAsync(
    std::string password,
    PhaseCallback on_phase,
    CompletionCallback on_complete,
    const std::chrono::milliseconds timeout)
{
    Cancel();
    if (m_worker.joinable()) {
        m_worker.join();
    }
    m_cancelled.store(false);
    m_worker = std::jthread([this, password = std::move(password), on_phase = std::move(on_phase), on_complete = std::move(on_complete), timeout]() mutable {
        const auto result = CreateIdentitySync(std::move(password), on_phase, timeout);
        if (on_complete) {
            on_complete(result);
        }
    });
}

IdentityCreationResult CybouIdentityService::RestoreIdentitySync(
    const RecoveryWords& words,
    std::string password,
    const PhaseCallback& on_phase,
    const std::chrono::milliseconds timeout)
{
    PasswordWiper wipe_password{password};
    (void)timeout;
    m_cancelled.store(false);
    m_phase.store(IdentityCreationPhase::CREATING_KEYS);
    if (on_phase) on_phase(IdentityCreationPhase::CREATING_KEYS, "Resolving Identity from verified state...");
    auto entropy = DecodeRecoveryWords(words);
    if (!entropy) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Invalid 24-word recovery phrase");
    }
    struct EntropyWiper {
        RecoveryEntropy& value;
        ~EntropyWiper() { crypto::CleanseMemory(value.data(), value.size()); }
    } wipe_entropy{*entropy};

    const auto recovery_key = DeriveIdentityPublicKey(*entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto authorization_key = DeriveIdentityPublicKey(*entropy, IdentityKeyPurpose::AUTHORIZATION);
    auto xwing_seed = DeriveIdentityXWingSeed(*entropy);
    const auto xwing_public = xwing_seed ? DeriveXWingPublicKey(*xwing_seed) : std::nullopt;
    if (xwing_seed) crypto::CleanseMemory(xwing_seed->data(), xwing_seed->size());
    const auto package = xwing_public ? EncodeIdentityKemPackage(*xwing_public) : std::nullopt;
    const auto recovery_id = recovery_key ? ComputeRecoveryKeyId(*recovery_key) : std::nullopt;
    if (!recovery_key || !authorization_key || !package || !recovery_id) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cannot derive Identity key roles");
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto account = loaded && loaded.state ? loaded.state->identities.FindByRecoveryKeyId(*recovery_id) : std::nullopt;
    if (!account && loaded && loaded.state) {
        // A genesis allocation names this recovery key but no Identity claimed
        // it yet: create that Identity from these words (fresh random
        // AccountID); its finalized AccountCreate claims the allocation.
        const auto allocation = loaded.state->genesis_allocations.find(*recovery_id);
        if (allocation != loaded.state->genesis_allocations.end() && !allocation->second.claimed_by) {
            {
                std::lock_guard lock(m_mutex);
                auto material = GenerateIdentityMaterial();
                if (!m_storage_path || std::filesystem::exists(*m_storage_path) || m_vault_saved || !material) {
                    m_phase.store(IdentityCreationPhase::FAILED);
                    return Failure(IdentityCreationPhase::FAILED, "Cannot prepare the genesis Identity on this computer");
                }
                material->recovery_entropy = *entropy;
                if (!m_keystore.LoadMaterial(std::move(*material))) {
                    m_phase.store(IdentityCreationPhase::FAILED);
                    return Failure(IdentityCreationPhase::FAILED, "Cannot derive Identity key roles");
                }
            }
            return CreateIdentitySync(std::move(password), on_phase, timeout);
        }
    }
    if (!account || !loaded || !loaded.state) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Recovery phrase is not in verified state; finish synchronization first");
    }
    const auto* record = loaded.state->identities.Find(*account);
    const auto network_id = m_runtime.GetNetworkId();
    const auto account_bytes = account->Value();
    const auto package_id = record ? ComputeIdentityKemPackageCommitment(
        std::span<const unsigned char, 32>{network_id.begin(), 32},
        std::span<const unsigned char, 32>{account_bytes.begin(), 32}, record->key_epoch, *package) : std::nullopt;
    if (!record || record->recovery_key != *recovery_key || record->authorization_key != *authorization_key ||
        !package_id || *package_id != record->kem_package_id) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Phrase does not derive the current finalized Identity key epoch", *account);
    }

    {
        std::lock_guard lock(m_mutex);
        if (!m_storage_path) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Portable vault path is not configured", *account);
        }
        if (password.size() < 12) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Vault password must have at least 12 characters", *account);
        }
        const auto& vault_path = *m_storage_path;
        if (std::filesystem::exists(vault_path)) {
            if (!m_keystore.LoadFromFile(vault_path, password)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Cannot unlock existing recovery vault", *account);
            }
        } else {
            IdentityMaterial material;
            std::copy(account_bytes.begin(), account_bytes.end(), material.account_id.begin());
            material.recovery_entropy = *entropy;
            if (!m_keystore.LoadMaterial(std::move(material)) || !m_keystore.SaveToFile(vault_path, password)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Cannot durably save and verify portable recovery vault", *account);
            }
        }
        if (m_keystore.GetAccountId() != account || m_keystore.GetRecoveryPublicKey() != recovery_key ||
            m_keystore.GetAuthorizationPublicKey() != authorization_key ||
            m_keystore.GetIdentityXWingPublicKey() != xwing_public) {
            m_keystore.Clear();
            m_vault_saved = false;
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Vault does not match recovery phrase or AccountID", *account);
        }
        m_vault_saved = true;
    }

    if (!IsAuthorizedIdentity(m_runtime, *account, m_keystore)) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Restored keys do not match finalized Identity capabilities", *account);
    }
    const auto account_state = m_runtime.GetAccountState(*account);
    m_phase.store(IdentityCreationPhase::ACTIVE);
    if (on_phase) on_phase(IdentityCreationPhase::ACTIVE, "Identity restored.");
    return IdentityCreationResult{true, IdentityCreationPhase::ACTIVE, *account,
        account_state ? account_state->creation_height : 0,
        account_state ? account_state->system_balance : 0, {}};
}
IdentityOperationResult CybouIdentityService::RotateIdentitySync(
    const RecoveryWords& new_words, std::string password)
{
    PasswordWiper wipe_password{password};
    std::lock_guard lock(m_mutex);
    if (!m_storage_path || !m_vault_saved || !m_keystore.HasKey()) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Unlock the active identity vault before rotation"};
    }
    if (password.size() < 12) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Vault password must have at least 12 characters"};
    }
    auto entropy = DecodeRecoveryWords(new_words);
    if (!entropy) return {.phase = IdentityOperationPhase::REJECTED, .error = "New recovery phrase is invalid"};
    struct EntropyWiper {
        RecoveryEntropy& value;
        ~EntropyWiper() { crypto::CleanseMemory(value.data(), value.size()); }
    } wipe_entropy{*entropy};

    const auto new_root = DeriveIdentityPublicKey(*entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto active_root = m_keystore.GetRecoveryPublicKey();
    if (!new_root || !active_root) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Recovery root keys are unavailable"};
    }
    auto& coordinator = m_runtime.GetIdentityOperationCoordinator(m_keystore);
    if (*new_root == *active_root) {
        return {.phase = IdentityOperationPhase::REJECTED,
            .error = "New recovery phrase matches the active Identity"};
    }

    auto active_material = LoadIdentityMaterial(*m_storage_path, password);
    const auto active_account = m_keystore.GetAccountId();
    const auto material_account = active_material ? AccountId::FromBytes(active_material->account_id) : std::nullopt;
    const auto material_root = active_material ? DeriveIdentityPublicKey(
        active_material->recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT) : std::nullopt;
    if (!active_material || !active_account || material_account != active_account ||
        !material_root || *material_root != *active_root) {
        return {.phase = IdentityOperationPhase::REJECTED,
            .error = "Active vault password or recovery identity does not match"};
    }

    auto candidate_material = m_keystore.CreateIdentityRotationMaterial(*entropy);
    auto expected_payload = candidate_material ? SerializeIdentityMaterial(*candidate_material) : std::nullopt;
    if (!candidate_material || !expected_payload) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Cannot prepare candidate recovery vault"};
    }
    struct PayloadWiper {
        std::vector<unsigned char>& value;
        ~PayloadWiper() { if (!value.empty()) crypto::CleanseMemory(value.data(), value.size()); }
    } wipe_payload{*expected_payload};

    auto candidate_path = *m_storage_path;
    candidate_path += ".rotation-pending";
    std::error_code ec;
    bool candidate_owned{false};
    if (std::filesystem::exists(candidate_path, ec)) {
        if (ec) return {.phase = IdentityOperationPhase::CONFLICT, .error = "Cannot inspect pending recovery vault"};
        auto existing_payload = LoadIdentityVault(candidate_path, password);
        if (!existing_payload) return {.phase = IdentityOperationPhase::CONFLICT,
            .error = "A pending recovery vault exists but cannot be authenticated"};
        const bool exact = *existing_payload == *expected_payload;
        crypto::CleanseMemory(existing_payload->data(), existing_payload->size());
        if (!exact) return {.phase = IdentityOperationPhase::CONFLICT,
            .error = "Pending recovery vault does not match the requested rotation"};
        candidate_owned = true;
    } else {
        if (ec || !SaveNewIdentityMaterial(candidate_path, password, *candidate_material)) {
            return {.phase = IdentityOperationPhase::REJECTED,
                .error = "Could not durably save and verify the candidate recovery vault"};
        }
        candidate_owned = true;
    }

    auto result = coordinator.RotateIdentity(*entropy);
    if (result.phase == IdentityOperationPhase::FINALIZED) {
        if (!PromoteIdentityVault(candidate_path, *m_storage_path, password, *expected_payload) ||
            !m_keystore.LoadFromFile(*m_storage_path, password)) {
            return {.phase = IdentityOperationPhase::CONFLICT, .op_id = result.op_id,
                .finalized_height = result.finalized_height,
                .error = "Recovery rotation finalized but the candidate vault could not be promoted and loaded"};
        }
        const auto promoted_root = m_keystore.GetRecoveryPublicKey();
        const auto finalized = m_runtime.GetStore().LoadState();
        const auto* finalized_identity = finalized && finalized.state && active_account ?
            finalized.state->identities.Find(*active_account) : nullptr;
        if (!promoted_root || *promoted_root != *new_root || !finalized_identity ||
            !coordinator.CompleteIdentityRotation(*finalized_identity)) {
            return {.phase = IdentityOperationPhase::CONFLICT, .op_id = result.op_id,
                .finalized_height = result.finalized_height,
                .error = "Candidate vault was promoted but recovery rotation reconciliation remains pending"};
        }
        m_vault_saved = true;
        result.error.clear();
    } else if (result.phase == IdentityOperationPhase::REJECTED && candidate_owned) {
        std::filesystem::remove(candidate_path, ec);
        if (ec) return {.phase = IdentityOperationPhase::CONFLICT, .op_id = result.op_id,
            .error = "Rotation was rejected but its candidate vault could not be removed"};
    }
    return result;
}

bool CybouIdentityService::HasPendingIdentityRotation()
{
    std::lock_guard lock(m_mutex);
    bool candidate_exists{false};
    if (m_storage_path) {
        auto candidate_path = *m_storage_path;
        candidate_path += ".rotation-pending";
        std::error_code ec;
        candidate_exists = std::filesystem::exists(candidate_path, ec) && !ec;
    }
    if (!candidate_exists && !m_keystore.HasKey()) return false;
    return candidate_exists || m_runtime.GetIdentityOperationCoordinator(
        m_keystore).HasPendingIdentityRotation();
}

IdentityOperationResult CybouIdentityService::ResumeIdentityRotationSync(std::string password)
{
    PasswordWiper wipe_password{password};
    const auto path = GetStoragePath();
    if (!path || !m_keystore.HasKey()) {
        return {.phase = IdentityOperationPhase::REJECTED, .error = "Unlock the identity vault before resuming rotation"};
    }
    auto candidate_path = *path;
    candidate_path += ".rotation-pending";
    std::error_code candidate_error;
    const bool candidate_exists = std::filesystem::exists(candidate_path, candidate_error);
    if (candidate_error) return {.phase = IdentityOperationPhase::CONFLICT,
        .error = "Cannot inspect the pending recovery vault"};
    auto& coordinator = m_runtime.GetIdentityOperationCoordinator(m_keystore);
    const auto active_root = m_keystore.GetRecoveryPublicKey();
    const auto account = m_keystore.GetAccountId();
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* record = loaded && loaded.state && account ? loaded.state->identities.Find(*account) : nullptr;
    if (!candidate_exists && active_root && record && record->recovery_key == *active_root) {
        if (!coordinator.HasPendingIdentityRotation() || coordinator.CompleteIdentityRotation(*record)) {
            return {.phase = IdentityOperationPhase::FINALIZED,
                .finalized_height = m_runtime.GetFinalizedHeight().value_or(0)};
        }
        if (coordinator.HasPendingIdentityRotation()) {
            return {.phase = IdentityOperationPhase::CONFLICT,
                .error = "The promoted recovery vault matches finalized state but its operation journal cannot be reconciled"};
        }
    }
    auto candidate = LoadIdentityMaterial(candidate_path, password);
    if (!candidate) return {.phase = IdentityOperationPhase::REJECTED,
        .error = "Pending recovery vault is missing or cannot be unlocked"};
    const auto candidate_account = AccountId::FromBytes(candidate->account_id);
    const auto active_account = m_keystore.GetAccountId();
    const auto candidate_recovery = DeriveIdentityPublicKey(candidate->recovery_entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto candidate_authorization = DeriveIdentityPublicKey(candidate->recovery_entropy, IdentityKeyPurpose::AUTHORIZATION);
    const auto candidate_active_authorization = m_keystore.GetAuthorizationPublicKey();
    if (!candidate_account || !active_account || *candidate_account != *active_account || !candidate_recovery ||
        !candidate_authorization || !candidate_active_authorization || !active_root || *candidate_recovery == *active_root ||
        *candidate_authorization == *candidate_active_authorization) {
        return {.phase = IdentityOperationPhase::CONFLICT,
            .error = "Pending Identity vault belongs to a different account or is not a new key epoch"};
    }
    auto words = EncodeRecoveryWords(candidate->recovery_entropy);
    for (const auto& word : words) {
        if (word.empty()) return {.phase = IdentityOperationPhase::REJECTED,
            .error = "Cannot decode the pending recovery phrase"};
    }
    auto result = RotateIdentitySync(words, std::move(password));
    for (auto& word : words) crypto::CleanseMemory(word.data(), word.size());
    return result;
}

void CybouIdentityService::RestoreIdentityAsync(
    RecoveryWords words, std::string password,
    PhaseCallback on_phase, CompletionCallback on_complete,
    const std::chrono::milliseconds timeout)
{
    Cancel();
    if (m_worker.joinable()) m_worker.join();
    m_cancelled.store(false);
    m_worker = std::jthread([this, words = std::move(words), password = std::move(password),
        on_phase = std::move(on_phase), on_complete = std::move(on_complete), timeout]() mutable {
        const auto result = RestoreIdentitySync(words, std::move(password), on_phase, timeout);
        for (auto& word : words) std::fill(word.begin(), word.end(), '\0');
        if (on_complete) on_complete(result);
    });
}

void CybouIdentityService::Cancel()
{
    m_cancelled.store(true);
}

} // namespace cybou
