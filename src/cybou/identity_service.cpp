// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/identity_service.h>
#include <openssl/rand.h>

#include <support/cleanse.h>

#include <algorithm>

namespace cybou {
namespace {
bool IsAuthorizedDevice(const CybouNodeRuntime& runtime, const AccountId& account_id, const CybouKeyStore& keystore);
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

bool CybouIdentityService::LoadVault(std::string_view password)
{
    std::lock_guard lock(m_mutex);
    if (!m_storage_path || !m_keystore.LoadFromFile(*m_storage_path, password)) return false;
    m_vault_saved = true;
    const auto acc_id = m_keystore.GetAccountId();
    if (!acc_id) return false;

    const auto account_state = m_runtime.GetAccountState(*acc_id);
    if (account_state && IsAuthorizedDevice(m_runtime, *acc_id, m_keystore)) {
        m_phase.store(IdentityCreationPhase::ACTIVE);
    } else {
        m_phase.store(IdentityCreationPhase::IDLE);
    }
    return true;
}

namespace {

struct PasswordWiper {
    std::string& value;
    ~PasswordWiper() { if (!value.empty()) memory_cleanse(value.data(), value.size()); }
};

bool IsAuthorizedDevice(const CybouNodeRuntime& runtime, const AccountId& account_id, const CybouKeyStore& keystore)
{
    const auto id = keystore.GetDeviceId();
    const auto key = keystore.GetDevicePublicKey();
    const auto loaded = runtime.GetStore().LoadState();
    if (!id || !key || !loaded || !loaded.state) return false;
    const auto* record = loaded.state->identities.Find(account_id);
    if (!record) return false;
    const auto found = record->devices.find(*id);
    return found != record->devices.end() && found->second.key == *key;
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
    m_cancelled.store(false);

    // Phase 1: CREATING_KEYS
    m_phase.store(IdentityCreationPhase::CREATING_KEYS);
    if (on_phase) on_phase(IdentityCreationPhase::CREATING_KEYS, "Generating local keys...");

    AccountId account_id;
    IdentityAuthorization auth;
    {
        std::lock_guard lock(m_mutex);
        if (!m_keystore.HasKey() || !m_storage_path) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Prepare identity and configure its vault before creation");
        }
        const auto acc_opt = m_keystore.GetAccountId();
        const auto dev_key = m_keystore.GetDevicePublicKey();
        const auto root_key = m_keystore.GetRecoveryPublicKey();
        if (!acc_opt || !dev_key || !root_key) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Invalid identity key");
        }
        account_id = *acc_opt;
        auth = IdentityAuthorization{
            .recovery_root = *root_key,
            .initial_device = *dev_key,
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
        if (!IsAuthorizedDevice(m_runtime, account_id, m_keystore)) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Local device is not authorized; resume identity restoration", account_id);
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

    const auto auth_commitment = ComputeIdentityAuthorizationCommitment(auth);
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

    const auto pop_digest = ComputeAccountCreatePopDigest(m_runtime.GetNetworkId(), account_id, auth);
    if (!pop_digest) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to compute proof of possession digest", account_id);
    }

    std::optional<IdentityHybridSignature> rec_pop;
    std::optional<IdentityHybridSignature> dev_pop;
    {
        std::lock_guard lock(m_mutex);
        rec_pop = m_keystore.SignRecovery(*pop_digest);
        dev_pop = m_keystore.SignDevice(*pop_digest);
    }
    if (!rec_pop || !dev_pop) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Failed to sign proof of possession", account_id);
    }

    AccountCreateOp op{
        .account_id = account_id,
        .authorization = auth,
        .work = work,
        .recovery_pop = *rec_pop,
        .device_pop = *dev_pop,
    };

    if (!m_runtime.SubmitOperation(ProtocolOperation{op})) {
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
    m_cancelled.store(false);
    m_phase.store(IdentityCreationPhase::CREATING_KEYS);
    if (on_phase) on_phase(IdentityCreationPhase::CREATING_KEYS, "Finding recovery root in verified state...");
    auto entropy = DecodeRecoveryWords(words);
    if (!entropy) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Invalid 24-word recovery phrase");
    }
    struct EntropyWiper {
        RecoveryEntropy& value;
        ~EntropyWiper() { memory_cleanse(value.data(), value.size()); }
    } wipe_entropy{*entropy};
    const auto root_key = DeriveIdentityPublicKey(*entropy, IdentityKeyPurpose::RECOVERY_ROOT);
    const auto root_id = root_key ? ComputeRecoveryKeyId(*root_key) : std::nullopt;
    if (!root_key || !root_id) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cannot derive recovery root");
    }
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto account = loaded && loaded.state ? loaded.state->identities.FindByRecoveryKeyId(*root_id) : std::nullopt;
    if (!account || !loaded || !loaded.state) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Recovery root is not in verified state; finish synchronization first");
    }
    const auto* record = loaded.state->identities.Find(*account);
    if (!record || record->recovery_root != *root_key) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Recovery root is unavailable", *account);
    }
    std::filesystem::path vault_path;
    {
        std::lock_guard lock(m_mutex);
        if (!m_storage_path) {
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Portable vault path is not configured", *account);
        }
        vault_path = *m_storage_path;
        if (std::filesystem::exists(vault_path)) {
            if (!m_keystore.LoadFromFile(vault_path, password)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Cannot unlock existing recovery vault", *account);
            }
            m_vault_saved = true;
        } else {
            if (record->devices.size() >= MAX_ACTIVE_DEVICES) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "The account has reached its device limit", *account);
            }
            if (password.size() < 12) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Vault password must have at least 12 characters", *account);
            }
            IdentityMaterial material;
            std::copy_n(account->Value().begin(), material.account_id.size(), material.account_id.begin());
            material.recovery_entropy = *entropy;
            if (RAND_priv_bytes(material.device_secret.data(), static_cast<int>(material.device_secret.size())) != 1) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Secure random generator failed", *account);
            }
            if (!m_keystore.LoadMaterial(std::move(material)) || !m_keystore.SaveToFile(vault_path, password)) {
                m_phase.store(IdentityCreationPhase::FAILED);
                return Failure(IdentityCreationPhase::FAILED, "Cannot save and verify recovery vault", *account);
            }
            m_vault_saved = true;
        }
        if (m_keystore.GetAccountId() != account || m_keystore.GetRecoveryPublicKey() != root_key) {
            m_keystore.Clear();
            m_vault_saved = false;
            m_phase.store(IdentityCreationPhase::FAILED);
            return Failure(IdentityCreationPhase::FAILED, "Vault does not match recovery phrase or AccountID", *account);
        }
    }
    if (IsAuthorizedDevice(m_runtime, *account, m_keystore)) {
        const auto account_state = m_runtime.GetAccountState(*account);
        m_phase.store(IdentityCreationPhase::ACTIVE);
        return IdentityCreationResult{true, IdentityCreationPhase::ACTIVE, *account,
            account_state ? account_state->creation_height : 0,
            account_state ? account_state->system_balance : 0, {}};
    }

    const auto device_key = m_keystore.GetDevicePublicKey();
    if (record->devices.size() >= MAX_ACTIVE_DEVICES) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "The account has reached its device limit", *account);
    }
    if (!device_key) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Recovered device key is unavailable", *account);
    }
    DeviceAdd request{.account_id = *account, .new_device = *device_key,
        .root_nonce = record->next_root_nonce, .root_signature = {}, .device_pop = {}};
    const auto digest = ComputeDeviceAddDigest(m_runtime.GetNetworkId(), request);
    if (!digest) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cannot sign device recovery", *account);
    }
    const auto root_signature = m_keystore.SignRecovery(*digest);
    const auto device_pop = m_keystore.SignDevice(*digest);
    if (!root_signature || !device_pop) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "Cannot sign device recovery", *account);
    }
    request.root_signature = *root_signature;
    request.device_pop = *device_pop;
    m_phase.store(IdentityCreationPhase::BROADCASTING);
    if (on_phase) on_phase(IdentityCreationPhase::BROADCASTING, "Submitting root-authorized DeviceAdd...");
    if (!m_runtime.SubmitOperation(ProtocolOperation{request})) {
        m_phase.store(IdentityCreationPhase::FAILED);
        return Failure(IdentityCreationPhase::FAILED, "DeviceAdd submission was rejected", *account);
    }
    m_phase.store(IdentityCreationPhase::WAITING_FOR_FINALITY);
    if (on_phase) on_phase(IdentityCreationPhase::WAITING_FOR_FINALITY, "Waiting for device authorization finality...");
    if (m_runtime.GetStatus().is_authority) m_runtime.ProduceBlock();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        if (IsAuthorizedDevice(m_runtime, *account, m_keystore)) {
            const auto account_state = m_runtime.GetAccountState(*account);
            m_phase.store(IdentityCreationPhase::ACTIVE);
            return IdentityCreationResult{true, IdentityCreationPhase::ACTIVE, *account,
                account_state ? account_state->creation_height : 0,
                account_state ? account_state->system_balance : 0, {}};
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    m_phase.store(IdentityCreationPhase::FAILED);
    return Failure(IdentityCreationPhase::FAILED,
        m_cancelled.load() ? "Cancelled" : "Timed out waiting for device authorization finality", *account);
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
