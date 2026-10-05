// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Реализация синхронного workflow claim'а имени .cybou.

#include <cybou/name_service.h>

#include <cybou/identity_vault.h>
#include <cybou/identity_material.h>
#include <cybou/name_registry.h>
#include <cybou/protocol_operation.h>
#include <cybou/crypto/cleanse.h>

#include <openssl/rand.h>

#include <algorithm>
#include <thread>

namespace cybou {
namespace {
constexpr size_t CLAIM_SIZE{4 + 32 + 32 + 1 + 32 + 32};
constexpr char MAGIC[]{"CYNC"};

struct ClaimSecret {
    std::string label;
    std::array<unsigned char, 32> salt{};
    ClaimSecret() = default;
    ClaimSecret(const ClaimSecret&) = delete;
    ClaimSecret& operator=(const ClaimSecret&) = delete;
    ClaimSecret(ClaimSecret&& other) noexcept : label{std::move(other.label)}, salt{other.salt}
    {
        crypto::CleanseMemory(other.salt.data(), other.salt.size());
    }
    ClaimSecret& operator=(ClaimSecret&& other) noexcept
    {
        if (this != &other) {
            crypto::CleanseMemory(salt.data(), salt.size());
            label = std::move(other.label);
            salt = other.salt;
            crypto::CleanseMemory(other.salt.data(), other.salt.size());
        }
        return *this;
    }
    ~ClaimSecret() { crypto::CleanseMemory(salt.data(), salt.size()); }
};

NameClaimResult Fail(std::string message)
{
    return {false, NameClaimPhase::FAILED, std::move(message)};
}

std::optional<ClaimSecret> LoadClaim(const std::filesystem::path& path, std::string_view password,
    const cybou::Hash256& network, const AccountId& account)
{
    auto bytes = LoadIdentityVault(path, password);
    if (!bytes) return std::nullopt;
    const bool valid = bytes->size() == CLAIM_SIZE &&
        std::equal(bytes->begin(), bytes->begin() + 4, MAGIC) &&
        std::equal(network.begin(), network.end(), bytes->begin() + 4) &&
        std::equal(account.Value().begin(), account.Value().end(), bytes->begin() + 36) &&
        (*bytes)[68] >= NAME_MIN_LABEL_LENGTH && (*bytes)[68] <= NAME_MAX_LABEL_LENGTH;
    std::optional<ClaimSecret> claim;
    if (valid) {
        ClaimSecret value;
        value.label.assign(reinterpret_cast<const char*>(bytes->data() + 69), (*bytes)[68]);
        std::copy_n(bytes->begin() + 101, 32, value.salt.begin());
        if (ValidateNameLabel(value.label) == NameValidationError::NONE &&
            std::all_of(bytes->begin() + 69 + value.label.size(), bytes->begin() + 101,
                [](unsigned char b) { return b == 0; }) &&
            std::any_of(value.salt.begin(), value.salt.end(), [](unsigned char b) { return b != 0; })) {
            claim.emplace(std::move(value));
        }
    }
    crypto::CleanseMemory(bytes->data(), bytes->size());
    return claim;
}

bool SaveClaim(const std::filesystem::path& path, std::string_view password,
    const cybou::Hash256& network, const AccountId& account, const ClaimSecret& claim)
{
    std::array<unsigned char, CLAIM_SIZE> payload{};
    std::copy_n(MAGIC, 4, payload.begin());
    std::copy_n(network.begin(), 32, payload.begin() + 4);
    std::copy_n(account.Value().begin(), 32, payload.begin() + 36);
    payload[68] = static_cast<unsigned char>(claim.label.size());
    std::copy(claim.label.begin(), claim.label.end(), payload.begin() + 69);
    std::copy(claim.salt.begin(), claim.salt.end(), payload.begin() + 101);
    const bool saved = SaveNewIdentityVault(path, password, payload);
    crypto::CleanseMemory(payload.data(), payload.size());
    if (!saved) return false;
    const auto reopened = LoadClaim(path, password, network, account);
    return reopened && reopened->label == claim.label && reopened->salt == claim.salt;
}

} // namespace

CybouNameService::CybouNameService(CybouNodeRuntime& runtime, CybouKeyStore& keystore,
    std::filesystem::path identity_vault_path)
    : m_runtime{runtime}, m_keystore{keystore},
      m_operation_coordinator{runtime.GetIdentityOperationCoordinator(keystore)},
      m_identity_vault_path{std::move(identity_vault_path)},
      m_claim_path{m_identity_vault_path}
{
    m_claim_path += ".nameclaim";
}

NameClaimResult CybouNameService::ClaimSync(std::string label, std::string password,
    const NamePhaseCallback& on_phase, std::chrono::milliseconds timeout)
{
    struct PasswordWiper { std::string& value; ~PasswordWiper() { crypto::CleanseMemory(value.data(), value.size()); } } wipe{password};
    m_cancelled.store(false);
    if (ValidateNameLabel(label) != NameValidationError::NONE) return Fail("Invalid .cybou label");
    const auto account = m_keystore.GetAccountId();
    if (!account) return Fail("Unlock an identity first");
    const auto vault_material = LoadIdentityMaterial(m_identity_vault_path, password);
    if (!vault_material || AccountId::FromBytes(vault_material->account_id) != account) {
        return Fail("Identity vault password is incorrect or vault does not match");
    }
    const auto network = m_runtime.GetNetworkBinding();
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    auto state = m_runtime.GetStore().GetStateSnapshot();
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
        if (!digest) return Fail("NameCommit commitment failed");
        const auto submitted = m_operation_coordinator.Execute(IdentityOperationKind::NAME_COMMIT, *digest,
            [&](const IdentityOperationAuthorization& authorization) -> std::optional<ProtocolOperation> {
                return ProtocolOperation{AuthorizedNameCommit{authorization, payload}};
            });
        if (!submitted) {
            return Fail(submitted.error.empty() ? "NameCommit submission failed" : submitted.error);
        }
        if (m_runtime.GetStatus().poa_signer_active) m_runtime.ProduceBlock();
    }
    if (on_phase) on_phase(NameClaimPhase::WAITING_FOR_COMMIT, "Waiting for finalized NameCommit...");
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        state = m_runtime.GetStore().GetStateSnapshot();
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
    const auto& params = m_runtime.GetNetworkGenesis().GetProtocolParameters();
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline &&
        m_runtime.GetFinalizedHeight().value_or(0) + 1 < commit_height + params.name_commit_min_depth) {
        if (m_runtime.GetStatus().poa_signer_active) m_runtime.ProduceBlock();
        else std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (m_cancelled.load() || std::chrono::steady_clock::now() >= deadline) return Fail("Waiting for commit depth timed out");
    if (on_phase) on_phase(NameClaimPhase::WORKING, "Computing NameClaimWork...");
    NameClaimWork work{.network_binding = network, .account_id = *account, .commitment = commitment,
        .work_epoch = EpochForHeight(m_runtime.GetFinalizedHeight().value_or(0) + 1, params)};
    while (!m_cancelled.load() && !CheckNameClaimWork(work, params.name_claim_work_bits)) ++work.nonce;
    if (m_cancelled.load()) return Fail("Name claim cancelled");
    NameRevealPayload reveal{.label = label, .salt = claim->salt, .work = work};
    const auto digest = ComputeNameRevealPayloadCommitment(reveal);
    if (!digest) return Fail("NameReveal commitment failed");
    if (on_phase) on_phase(NameClaimPhase::REVEALING, "Submitting NameReveal...");
    const auto submitted = m_operation_coordinator.Execute(IdentityOperationKind::NAME_REVEAL, *digest,
        [&](const IdentityOperationAuthorization& authorization) -> std::optional<ProtocolOperation> {
            return ProtocolOperation{AuthorizedNameReveal{authorization, reveal}};
        });
    if (!submitted) {
        return Fail(submitted.error.empty() ? "NameReveal submission failed" : submitted.error);
    }
    if (m_runtime.GetStatus().poa_signer_active) m_runtime.ProduceBlock();
    if (on_phase) on_phase(NameClaimPhase::WAITING_FOR_NAME, "Waiting for finalized name ownership...");
    while (!m_cancelled.load() && std::chrono::steady_clock::now() < deadline) {
        state = m_runtime.GetStore().GetStateSnapshot();
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
