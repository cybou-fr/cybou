// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/name_service.h>

#include <cybou/identity_vault.h>
#include <cybou/name_registry.h>
#include <cybou/protocol_operation.h>
#include <support/cleanse.h>

#include <openssl/rand.h>

#include <algorithm>
#include <thread>

namespace cybou {
namespace {
constexpr size_t CLAIM_SIZE{5 + 32 + 32 + 1 + 32 + 32};
constexpr char MAGIC[]{"CYNC2"};

struct ClaimSecret {
    std::string label;
    std::array<unsigned char, 32> salt{};
    ClaimSecret() = default;
    ClaimSecret(const ClaimSecret&) = delete;
    ClaimSecret& operator=(const ClaimSecret&) = delete;
    ClaimSecret(ClaimSecret&& other) noexcept : label{std::move(other.label)}, salt{other.salt}
    {
        memory_cleanse(other.salt.data(), other.salt.size());
    }
    ClaimSecret& operator=(ClaimSecret&& other) noexcept
    {
        if (this != &other) {
            memory_cleanse(salt.data(), salt.size());
            label = std::move(other.label);
            salt = other.salt;
            memory_cleanse(other.salt.data(), other.salt.size());
        }
        return *this;
    }
    ~ClaimSecret() { memory_cleanse(salt.data(), salt.size()); }
};

NameClaimResult Fail(std::string message)
{
    return {false, NameClaimPhase::FAILED, std::move(message)};
}

std::optional<ClaimSecret> LoadClaim(const std::filesystem::path& path, std::string_view password,
    const uint256& network, const AccountId& account)
{
    auto bytes = LoadIdentityVault(path, password);
    if (!bytes) return std::nullopt;
    const bool valid = bytes->size() == CLAIM_SIZE &&
        std::equal(bytes->begin(), bytes->begin() + 5, MAGIC) &&
        std::equal(network.begin(), network.end(), bytes->begin() + 5) &&
        std::equal(account.Value().begin(), account.Value().end(), bytes->begin() + 37) &&
        (*bytes)[69] >= NAME_MIN_LABEL_LENGTH && (*bytes)[69] <= NAME_MAX_LABEL_LENGTH;
    std::optional<ClaimSecret> claim;
    if (valid) {
        ClaimSecret value;
        value.label.assign(reinterpret_cast<const char*>(bytes->data() + 70), (*bytes)[69]);
        std::copy_n(bytes->begin() + 102, 32, value.salt.begin());
        if (ValidateNameLabel(value.label) == NameValidationError::NONE &&
            std::all_of(bytes->begin() + 70 + value.label.size(), bytes->begin() + 102,
                [](unsigned char b) { return b == 0; }) &&
            std::any_of(value.salt.begin(), value.salt.end(), [](unsigned char b) { return b != 0; })) {
            claim.emplace(std::move(value));
        }
    }
    memory_cleanse(bytes->data(), bytes->size());
    return claim;
}

bool SaveClaim(const std::filesystem::path& path, std::string_view password,
    const uint256& network, const AccountId& account, const ClaimSecret& claim)
{
    std::array<unsigned char, CLAIM_SIZE> payload{};
    std::copy_n(MAGIC, 5, payload.begin());
    std::copy_n(network.begin(), 32, payload.begin() + 5);
    std::copy_n(account.Value().begin(), 32, payload.begin() + 37);
    payload[69] = static_cast<unsigned char>(claim.label.size());
    std::copy(claim.label.begin(), claim.label.end(), payload.begin() + 70);
    std::copy(claim.salt.begin(), claim.salt.end(), payload.begin() + 102);
    const bool saved = SaveNewIdentityVault(path, password, payload);
    memory_cleanse(payload.data(), payload.size());
    if (!saved) return false;
    const auto reopened = LoadClaim(path, password, network, account);
    return reopened && reopened->label == claim.label && reopened->salt == claim.salt;
}

std::optional<DeviceAuthorization> SignOperation(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
    const AccountId& account, DeviceOperationKind kind, const IdentityKeyId& payload_commitment)
{
    const auto device_id = keystore.GetDeviceId();
    const auto loaded = runtime.GetStore().LoadState();
    if (!device_id || !loaded || !loaded.state) return std::nullopt;
    const auto* record = loaded.state->identities.Find(account);
    if (!record) return std::nullopt;
    const auto it = record->devices.find(*device_id);
    if (it == record->devices.end() || it->second.key != keystore.GetDevicePublicKey()) return std::nullopt;
    DeviceAuthorization auth{.account_id = account, .device_id = *device_id,
        .nonce = it->second.next_nonce, .activation_nonce = it->second.activation_nonce,
        .kind = kind, .payload_commitment = payload_commitment, .signature = {}};
    const auto digest = ComputeDeviceOperationDigest(runtime.GetNetworkId(), auth);
    const auto signature = digest ? keystore.SignDevice(*digest) : std::nullopt;
    if (!signature) return std::nullopt;
    auth.signature = *signature;
    return auth;
}
} // namespace

CybouNameService::CybouNameService(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
    std::filesystem::path identity_vault_path)
    : m_runtime{runtime}, m_keystore{keystore}, m_claim_path{std::move(identity_vault_path)}
{
    m_claim_path += ".nameclaim";
}

NameClaimResult CybouNameService::ClaimSync(std::string label, std::string password,
    const NamePhaseCallback& on_phase, std::chrono::milliseconds timeout)
{
    struct PasswordWiper { std::string& value; ~PasswordWiper() { memory_cleanse(value.data(), value.size()); } } wipe{password};
    m_cancelled.store(false);
    if (ValidateNameLabel(label) != NameValidationError::NONE) return Fail("Invalid .cybou label");
    const auto account = m_keystore.GetAccountId();
    if (!account) return Fail("Unlock an identity first");
    const auto network = m_runtime.GetNetworkId();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    auto state = m_runtime.GetStore().LoadState();
    if (!state || !state.state || !state.state->accounts.contains(*account)) return Fail("Account is not finalized");
    if (const auto* owned = state.state->names.PrimaryName(*account)) {
        return *owned == label ? NameClaimResult{true, NameClaimPhase::ACTIVE, {}} : Fail("Account already owns a name");
    }
    if (state.state->names.Resolve(label)) return Fail("Name is already owned");
    if (on_phase) on_phase(NameClaimPhase::SAVING, "Saving encrypted name claim...");

    std::optional<ClaimSecret> claim;
    if (std::filesystem::exists(m_claim_path)) {
        claim = LoadClaim(m_claim_path, password, network, *account);
        if (!claim || claim->label != label) return Fail("Existing encrypted name claim cannot be unlocked or differs");
    } else {
        if (password.size() < 12) return Fail("Vault password must have at least 12 characters");
        ClaimSecret value;
        value.label = label;
        if (RAND_bytes(value.salt.data(), value.salt.size()) != 1 ||
            std::all_of(value.salt.begin(), value.salt.end(), [](unsigned char b) { return b == 0; }) ||
            !SaveClaim(m_claim_path, password, network, *account, value)) return Fail("Cannot save encrypted name claim");
        claim.emplace(std::move(value));
    }
    const auto commitment = ComputeNameCommitment(network, *account, label, claim->salt);
    auto pending = state.state->names.pending_commits.find(commitment);
    if (pending == state.state->names.pending_commits.end()) {
        for (const auto& [hash, record] : state.state->names.pending_commits) {
            if (record.account_id == *account) return Fail("Another name claim is pending for this account");
        }
        if (on_phase) on_phase(NameClaimPhase::COMMITTING, "Submitting NameCommit...");
        NameCommitPayload payload{.commitment = commitment};
        const auto digest = ComputeNameCommitPayloadCommitment(payload);
        const auto auth = digest ? SignOperation(m_runtime, m_keystore, *account,
            DeviceOperationKind::NAME_COMMIT, *digest) : std::nullopt;
        if (!auth || !m_runtime.SubmitOperation(ProtocolOperation{AuthorizedNameCommit{*auth, payload}})) {
            return Fail("NameCommit submission failed");
        }
        if (m_runtime.GetStatus().is_authority) m_runtime.ProduceBlock();
    }
    if (on_phase) on_phase(NameClaimPhase::WAITING_FOR_COMMIT, "Waiting for finalized NameCommit...");
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        state = m_runtime.GetStore().LoadState();
        if (state && state.state) {
            pending = state.state->names.pending_commits.find(commitment);
            if (pending != state.state->names.pending_commits.end()) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (m_cancelled.load() || !state || !state.state || pending == state.state->names.pending_commits.end()) {
        return Fail("NameCommit is not finalized yet; retry with the same label and password");
    }
    const auto commit_height = pending->second.commit_height;
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline &&
        m_runtime.GetFinalizedHeight().value_or(0) + 1 < commit_height + params.name_commit_min_depth) {
        if (m_runtime.GetStatus().is_authority) m_runtime.ProduceBlock();
        else std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (m_cancelled.load() || std::chrono::steady_clock::now() >= deadline) return Fail("Waiting for commit depth timed out");
    if (on_phase) on_phase(NameClaimPhase::WORKING, "Computing NameClaimWork...");
    NameClaimWork work{.network_id = network, .account_id = *account, .commitment = commitment,
        .work_epoch = EpochForHeight(m_runtime.GetFinalizedHeight().value_or(0) + 1, params)};
    while (!m_cancelled.load() && !CheckNameClaimWork(work, params.name_claim_work_bits)) ++work.nonce;
    if (m_cancelled.load()) return Fail("Name claim cancelled");
    NameRevealPayload reveal{.label = label, .salt = claim->salt, .work = work};
    const auto digest = ComputeNameRevealPayloadCommitment(reveal);
    const auto auth = digest ? SignOperation(m_runtime, m_keystore, *account,
        DeviceOperationKind::NAME_REVEAL, *digest) : std::nullopt;
    if (!auth) return Fail("Cannot sign NameReveal");
    if (on_phase) on_phase(NameClaimPhase::REVEALING, "Submitting NameReveal...");
    if (!m_runtime.SubmitOperation(ProtocolOperation{AuthorizedNameReveal{*auth, reveal}})) {
        return Fail("NameReveal submission failed");
    }
    if (m_runtime.GetStatus().is_authority) m_runtime.ProduceBlock();
    if (on_phase) on_phase(NameClaimPhase::WAITING_FOR_NAME, "Waiting for finalized name ownership...");
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        state = m_runtime.GetStore().LoadState();
        if (state && state.state) {
            const auto* owned = state.state->names.PrimaryName(*account);
            if (owned && *owned == label) return {true, NameClaimPhase::ACTIVE, {}};
            if (owned || state.state->names.Resolve(label)) return Fail("Name was finalized for another account");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return Fail("NameReveal is not finalized yet; retry with the same label and password");
}

} // namespace cybou
