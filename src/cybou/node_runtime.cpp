// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

/// \file
/// \brief Реализация потокобезопасного Full Node runtime: state, relay, sync и storage.

#include <cybou/operation_submit.h>
#include <cybou/node_runtime.h>
#include <cybou/operation_work.h>
#include <cybou/secret_file.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/p2p/session.h>
#include <cybou/identity_crypto.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/p2p/peer_manager.h>
#include <cybou/p2p/peer_admission.h>

#include <boost/asio/ip/address.hpp>
#include <openssl/rand.h>

#include <algorithm>
#include <fstream>
#include <limits>
#include <string_view>

namespace cybou {

NodeRuntimeConfig MakeNodeRuntimeConfig(const OfficialNetwork& network, const std::filesystem::path& data_dir)
{
    NodeRuntimeConfig config{.network_genesis = network.genesis, .data_dir = data_dir};
    config.configured_peers.reserve(network.rendezvous_locators.size());
    for (const auto& locator : network.rendezvous_locators)
        config.configured_peers.push_back({{std::string{locator.host}, locator.port}, locator.tls_spki_sha256});
    return config;
}


namespace {

/// \brief Форматирует ChunkId в lower-case hex для diagnostics/event log.
std::string ChunkIdHex(const ChunkId& id)
{
    static constexpr char digits[]="0123456789abcdef";
    std::string out;
    out.reserve(id.size()*2);
    for (auto byte : id) { out+=digits[byte>>4]; out+=digits[byte&15]; }
    return out;
}

// Discovered routing hints are untrusted. Reject address scopes this node has
// no business dialing: unspecified, multicast, and (unless the local CYBOU P2P
// listener lives in the same scope) loopback and link-local targets. Without a
// known local listener the policy stays permissive for DEV tooling.
/// \brief Определяет private/ULA адреса, которые нельзя безусловно dial'ить по чужим routing hints.
bool IsPrivateAddress(const boost::asio::ip::address& addr)
{
    const auto is_private_v4 = [](const uint32_t value) {
        return (value & 0xFF000000U) == 0x0A000000U ||
            (value & 0xFFF00000U) == 0xAC100000U ||
            (value & 0xFFFF0000U) == 0xC0A80000U;
    };
    if (addr.is_v4()) {
        return is_private_v4(addr.to_v4().to_uint());
    }
    const auto bytes = addr.to_v6().to_bytes();
    if (addr.to_v6().is_v4_mapped()) {
        const uint32_t value = (static_cast<uint32_t>(bytes[12]) << 24) |
            (static_cast<uint32_t>(bytes[13]) << 16) |
            (static_cast<uint32_t>(bytes[14]) << 8) | static_cast<uint32_t>(bytes[15]);
        return is_private_v4(value);
    }
    return (bytes[0] & 0xFEU) == 0xFCU; // Unique-local IPv6 (fc00::/7).
}

/// \brief Отфильтровывает discovered endpoints, которые выглядели бы loopback/link-local/private из чужой зоны.
bool IsConnectableDiscoveredAddress(
    const boost::asio::ip::address& addr,
    const std::optional<std::pair<std::string, uint16_t>>& advertised_endpoint)
{
    auto is_link_local = [](const boost::asio::ip::address& a) {
        if (a.is_v4()) return (a.to_v4().to_uint() & 0xFFFF0000U) == 0xA9FE0000U;
        const auto bytes = a.to_v6().to_bytes();
        return bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U;
    };
    if (addr.is_unspecified() || addr.is_multicast()) return false;
    if (IsPrivateAddress(addr) && advertised_endpoint) {
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        if (!ec && !IsPrivateAddress(local) && !local.is_loopback() && !is_link_local(local) &&
            !local.is_unspecified() && !local.is_multicast()) {
            return false;
        }
    }
    if (addr.is_loopback()) {
        if (!advertised_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        return !ec && local.is_loopback();
    }
    if (is_link_local(addr)) {
        if (!advertised_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(advertised_endpoint->first, ec);
        return !ec && is_link_local(local);
    }
    return true;
}

} // namespace

namespace {
/// \brief Загружает или создаёт стабильный storage secret узла.
/// \details 32 random bytes kept beside provider data (0600); in memory for memory-only runtimes.
std::optional<std::array<unsigned char, 32>> LoadOrCreateProviderSecret(const std::filesystem::path& path)
{
    std::array<unsigned char, 32> secret{};
    if (!path.empty() && std::filesystem::exists(path)) {
        auto bytes = ReadSecretFile(path, 32);
        if (!bytes || bytes->size() != secret.size()) return std::nullopt;
        std::copy(bytes->begin(), bytes->end(), secret.begin());
        crypto::CleanseMemory(bytes->data(), bytes->size());
        return secret;
    }
    if (RAND_bytes(secret.data(), static_cast<int>(secret.size())) != 1) return std::nullopt;
    if (path.empty()) return secret;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path());
    if (!CreateSecretFile(path, secret)) {
        crypto::CleanseMemory(secret.data(), secret.size());
        return std::nullopt;
    }
    return secret;
}
} // namespace

CybouNodeRuntime::CybouNodeRuntime(NodeRuntimeConfig config)
    : m_config{std::move(config)},
      m_network_binding{ComputeNetworkBinding(m_config.network_genesis.GetNetworkPublicKey())},
      m_db{std::make_unique<KVStore>(KVStoreOptions{
          .path = m_config.data_dir,
          .cache_bytes = m_config.db_cache_bytes,
          .memory_only = m_config.memory_only,
          .wipe_data = m_config.wipe_data,
      })},
      m_store{*m_db, m_config.network_genesis}
{
    std::filesystem::path storage_path;
    if (!m_config.memory_only) {
        storage_path = std::filesystem::path{m_config.data_dir.string() + ".chunks"};
    }
    // `V` — явная local policy оператора, не consensus state; минимум не ослабляется для production.
    if (!m_config.storage_capacity_bytes) m_config.storage_capacity_bytes = DEFAULT_STORAGE_CAPACITY_BYTES;
    if (*m_config.storage_capacity_bytes < MIN_STORAGE_CAPACITY_BYTES && !m_config.memory_only)
        throw std::invalid_argument("storage capacity must be at least 15 GiB; smaller values are memory-only tests");
    const uint64_t capacity = *m_config.storage_capacity_bytes;
    // Zero существует только для memory-only tests «без provider»: собственный staging не ограничен.
    m_chunk_blob_store = std::make_unique<ChunkBlobStore>(
        m_config.memory_only ? std::filesystem::path{} : storage_path / "chunks",
        m_config.memory_only, m_config.wipe_data,
        capacity == 0 ? std::numeric_limits<uint64_t>::max() : capacity);
    m_chunk_retention = std::make_unique<ChunkRetentionRegistry>(
        m_config.memory_only ? std::filesystem::path{} : storage_path / "retention",
        m_config.memory_only, m_config.wipe_data);
    m_finalized_chunk_store = std::make_unique<FinalizedChunkStore>(*m_chunk_blob_store, storage_path,
        std::span<const unsigned char, 32>{m_network_binding.begin(), 32},
        ProviderBudgetBytes(capacity), m_config.wipe_data);
    // Recover a crash between durable block commit and its local purge event.
    const auto restored = m_store.LoadState();
    if (restored.error == StateLoadError::NONE && restored.state) {
        m_finalized_chunk_store->PurgeRevokedPublications(
            [&](const cybou::Hash256& id) { return FindFinalizedRootPublication(id); });
    }
    // The provider key is this node's stable storage identity across restarts.
    m_storage_secret = LoadOrCreateProviderSecret(m_config.memory_only ? std::filesystem::path{} :
        storage_path / "storage.key");
    if (!m_storage_secret) throw std::runtime_error("cannot load or create the storage provider key");
    const auto storage_key = DeriveIdentityPublicKey(*m_storage_secret, IdentityKeyPurpose::STORAGE);
    if (storage_key) {
        constexpr std::string_view storage_id_domain{"CYBOU/STORAGE-ID"};
        std::vector<unsigned char> storage_id_input(storage_id_domain.begin(), storage_id_domain.end());
        storage_id_input.insert(storage_id_input.end(), storage_key->ed25519.begin(), storage_key->ed25519.end());
        storage_id_input.insert(storage_id_input.end(), storage_key->ml_dsa.begin(), storage_key->ml_dsa.end());
        m_storage_id = ComputeBlake3Digest(storage_id_input);
    }
    if (!m_storage_id) throw std::runtime_error("storage provider key is invalid");
    if (m_config.poa_finalizer_recovery_entropy.has_value()) {
        m_poa_finalizer = std::make_unique<PoaFinalizer>(
            m_store.GetDatabase(), m_store.GetNetworkBinding(), m_store.GetNetworkGenesis().GetGenesisAnchor(),
            m_config.poa_finalizer_recovery_entropy->Get(), m_store.GetNetworkGenesis().GetPoaPublicKey());
        m_config.poa_finalizer_recovery_entropy.reset();
    }
    // Keep the local full node usable before it has learned or connected to a
    // peer. Configured endpoints and discovered hints can be added later.
    m_peer_manager = std::make_unique<p2p::PeerManager>(*this);
}

bool CybouNodeRuntime::AdmitPeerAddress(const std::string& numeric_address) const
{
    return m_config.peer_admission_policy && m_config.peer_admission_policy->Ready() &&
        m_config.peer_admission_policy->Allows(numeric_address);
}

CybouNodeRuntime::~CybouNodeRuntime()
{
    if (m_storage_secret) crypto::CleanseMemory(m_storage_secret->data(), m_storage_secret->size());
}

std::optional<std::array<unsigned char, 32>> CybouNodeRuntime::LocalStorageId() const { return m_storage_id; }

ChunkRetentionRegistry::CollectResult CybouNodeRuntime::CollectChunkGarbage(const std::uint64_t cache_budget_bytes,
    const std::uint64_t now_ms, const std::size_t max_removals)
{
    // A freshly cached or released blob may be in active use by a download.
    constexpr std::uint64_t GRACE_MS{10 * 60 * 1000};
    (void)m_finalized_chunk_store->RetryPendingPurges(max_removals);
    return m_chunk_retention->Collect(*m_chunk_blob_store, cache_budget_bytes, now_ms, GRACE_MS, max_removals,
        [&](const ChunkId& id) {
            return m_finalized_chunk_store->RemoveUnlessAdmitted(id);
        });
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::SignStorageProof(
    const std::span<const unsigned char> message) const
{
    if (!m_storage_secret) return std::nullopt;
    const auto key = DeriveIdentityPublicKey(*m_storage_secret, IdentityKeyPurpose::STORAGE);
    const auto signature = SignIdentityMessage(*m_storage_secret, IdentityKeyPurpose::STORAGE, message);
    if (!key || !signature) return std::nullopt;
    std::vector<unsigned char> proof;
    proof.reserve(key->ed25519.size() + key->ml_dsa.size() + signature->ed25519.size() + signature->ml_dsa.size());
    proof.insert(proof.end(), key->ed25519.begin(), key->ed25519.end());
    proof.insert(proof.end(), key->ml_dsa.begin(), key->ml_dsa.end());
    proof.insert(proof.end(), signature->ed25519.begin(), signature->ed25519.end());
    proof.insert(proof.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
    return proof;
}

ChunkAdmissionResult CybouNodeRuntime::PutFinalizedChunk(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    // Provider принимает только оплаченное хранение: финализированная публикация с активной арендой (DEC-279).
    auto result = m_finalized_chunk_store->PutChunk(publication_operation_id, chunk_id, stored_bytes, proof,
        [this](const cybou::Hash256& operation_id) -> std::optional<RootPublication> {
            if (!IsStorageLeaseActive(operation_id)) return std::nullopt;
            return FindFinalizedRootPublication(operation_id);
        });
    // Receipt подписывается только после durable admission; без подписи admission не подтверждается.
    if (result) {
        auto receipt = SignStorageProof(StorageReceiptMessage(m_network_binding, publication_operation_id, chunk_id,
            static_cast<std::uint32_t>(stored_bytes.size())));
        if (receipt) result.receipt = std::move(*receipt);
        else result.status = ChunkAdmissionStatus::STORAGE_ERROR;
    }
    if (m_config.event_writer) m_config.event_writer->Write(result ? NodeEvent::chunk_put : NodeEvent::chunk_verify_failed,
        {{"operation_id",publication_operation_id.GetHex()},{"chunk_id",ChunkIdHex(chunk_id)},{"bytes",std::uint64_t{stored_bytes.size()}},
         {"error_code",std::uint64_t{static_cast<unsigned>(result.status)}}});
    return result;
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetFinalizedChunk(const ChunkId& chunk_id) const
{
    auto bytes = m_finalized_chunk_store->GetChunk(chunk_id);
    if (m_config.event_writer) m_config.event_writer->Write(bytes ? NodeEvent::chunk_get : NodeEvent::chunk_verify_failed,
        {{"chunk_id",ChunkIdHex(chunk_id)},{"bytes",std::uint64_t{bytes ? bytes->size() : 0}}});
    return bytes;
}

StorageAuditAnswer CybouNodeRuntime::AnswerStorageAudit(const StorageAuditChallenge& challenge) const
{
    // Отвечаем только по admitted provider-копии: локальный cache не является обязательством.
    const auto bytes = m_finalized_chunk_store->GetChunk(challenge.chunk_id);
    const auto proof = bytes ? CreateStorageAuditProof(challenge, *bytes) : std::nullopt;
    if (!proof) return {};
    return {.held = true, .response_hash = proof->response_hash};
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetFinalizedChunkAuthorizationProof(
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id) const
{
    return m_finalized_chunk_store->GetChunkAuthorizationProof(publication_operation_id, chunk_id,
        [this](const cybou::Hash256& operation_id) { return FindFinalizedRootPublication(operation_id); });
}

bool CybouNodeRuntime::HasFinalizedChunk(const ChunkId& chunk_id) const
{
    return m_finalized_chunk_store->HasChunk(chunk_id);
}

std::vector<CybouNodeRuntime::StorageEndpoint> CybouNodeRuntime::StorageEndpoints() const
{
    std::vector<p2p::PeerInfo> peers;
    {
        std::lock_guard p2p_lock(m_p2p_mutex);
        if (!m_peer_manager) return {};
        peers = m_peer_manager->StorageEndpoints();
    }
    std::optional<CybouState> state;
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded && loaded.state) state = std::move(*loaded.state);
    }
    std::vector<StorageEndpoint> endpoints;
    endpoints.reserve(peers.size());
    for (const auto& peer : peers) {
        if (!peer.storage_id) continue;
        StorageEndpoint endpoint{peer.address, peer.port, *peer.storage_id};
        // Payout-аккаунт признаётся только по текущему finalized Authorization-ключу аккаунта.
        if (peer.payout_binding && state) {
            const auto* record = state->identities.Find(peer.payout_binding->payout_account);
            const auto digest = StoragePayoutBindingDigest(m_network_binding, *peer.storage_id,
                peer.payout_binding->payout_account);
            if (record && VerifyIdentityMessage(record->authorization_key, peer.payout_binding->authorization, digest)) {
                endpoint.payout_account = peer.payout_binding->payout_account;
            }
        }
        endpoints.push_back(std::move(endpoint));
    }
    return endpoints;
}

std::optional<StoragePayoutBinding> CybouNodeRuntime::LocalStoragePayoutBinding() const
{
    IdentitySignerRef signer;
    {
        std::lock_guard lock{m_mutex};
        signer = m_identity_signer;
    }
    const auto account = signer ? signer->Account() : std::nullopt;
    const auto storage_id = LocalStorageId();
    if (!account || !storage_id) return std::nullopt;
    const auto digest = StoragePayoutBindingDigest(m_network_binding, *storage_id, *account);
    auto authorization = signer->SignAuthorization(digest);
    auto storage_proof = SignStorageProof(digest);
    if (!authorization || !storage_proof) return std::nullopt;
    return StoragePayoutBinding{.payout_account = *account, .authorization = std::move(*authorization),
        .storage_proof = std::move(*storage_proof)};
}

std::optional<ChunkAdmissionResult> CybouNodeRuntime::PutChunkToStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const cybou::Hash256& publication_operation_id,
    const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->PutAuthorizedChunk(address, port, storage_id, publication_operation_id, chunk_id,
        stored_bytes, proof);
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetChunkFromStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const ChunkId& chunk_id)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->GetChunkById(address, port, storage_id, chunk_id);
}

std::optional<StorageAuditAnswer> CybouNodeRuntime::AuditChunkAtStorageEndpoint(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& storage_id, const StorageAuditChallenge& challenge)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->AuditChunk(address, port, storage_id, challenge);
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetChunkAuthorizationProofFromStorageEndpoint(
    const std::string& address, const uint16_t port, const std::array<unsigned char, 32>& storage_id,
    const cybou::Hash256& publication_operation_id, const ChunkId& chunk_id)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->GetChunkAuthorizationProof(address, port, storage_id,
        publication_operation_id, chunk_id);
}

bool CybouNodeRuntime::InitializeGenesis(const CybouState& genesis, const bool sync)
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NONE && loaded.state.has_value()) {
        return true;
    }
    // Any non-NOT_FOUND load error is treated as authoritative here: a caller
    // must not overwrite corrupt or foreign-network data by "initializing again".
    if (loaded.error != StateLoadError::NOT_FOUND) {
        return false;
    }
    const auto result = m_store.InitializeGenesis(genesis, sync);
    return result.error == GenesisInitError::NONE;
}

NodeRuntimeStatus CybouNodeRuntime::GetStatus() const
{
    std::lock_guard lock(m_mutex);
    NodeRuntimeStatus status;
    status.network_binding = m_network_binding;
    status.poa_signer_active = m_poa_finalizer && m_poa_finalizer->SignerEnabled();

    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        status.runtime_state = NodeRuntimeState::NETWORK_MISMATCH;
    } else if (loaded.error == StateLoadError::CORRUPT) {
        status.runtime_state = NodeRuntimeState::CORRUPT;
    } else if (loaded.error == StateLoadError::NOT_FOUND) {
        status.runtime_state = NodeRuntimeState::UNINITIALIZED;
    } else if (loaded.error == StateLoadError::NONE && loaded.state.has_value()) {
        status.is_initialized = true;
        status.runtime_state = NodeRuntimeState::READY;
    }

    const auto head = m_store.GetFinalizedHead();
    if (head) {
        status.finalized_height = head->height;
        status.finalized_tip = head->block_id;
    }
    const auto root = m_store.GetStateRoot();
    if (root) {
        status.state_root = *root;
    }
    status.poa_safety_halted = m_production_status == BlockProductionStatus::SAFETY_HALT || m_store.PoaSafetyHalted() ||
        (m_poa_finalizer && m_poa_finalizer->SafetyHalted());
    // Safety halt intentionally dominates READY once state exists: UX must
    // surface fail-closed signing state even though finalized reads still work.
    if (status.poa_safety_halted && status.is_initialized) status.runtime_state = NodeRuntimeState::SAFETY_HALTED;
    return status;
}

PoaEvidenceReadResult CybouNodeRuntime::ReadPoaSafetyEvidence() const
{
    std::lock_guard lock(m_mutex);
    return m_store.ReadPoaSafetyEvidence();
}

std::optional<uint64_t> CybouNodeRuntime::GetFinalizedHeight() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetFinalizedHeight();
}

std::optional<cybou::Hash256> CybouNodeRuntime::GetFinalizedTip() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetFinalizedTip();
}

std::optional<cybou::Hash256> CybouNodeRuntime::GetStateRoot() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetStateRoot();
}

std::optional<AccountState> CybouNodeRuntime::GetAccountState(const AccountId& account_id) const
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (!loaded || !loaded.state) return std::nullopt;
    const auto it = loaded.state->accounts.find(account_id);
    if (it == loaded.state->accounts.end()) return std::nullopt;
    return it->second;
}

bool CybouNodeRuntime::IsTreasuryClaimant(const AccountId& account_id) const
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (!loaded || !loaded.state) return false;
    return std::any_of(loaded.state->genesis_allocations.begin(), loaded.state->genesis_allocations.end(),
        [&](const auto& entry) {
            return entry.second.label == CENTRAL_AUTHORITY_NAME && entry.second.claimed_by == account_id;
        });
}

std::optional<uint64_t> CybouNodeRuntime::PrepareOperationWork(const ProtocolOperation& op)
{
    // The originating node does the relay proof-of-work (DEC-273) outside the
    // runtime lock: it is meant to take noticeable time on the author's machine.
    // Without finalized state the author counts as T0, the hardest tier.
    const auto op_id = ComputeOperationId(op);
    if (!op_id) return std::nullopt;
    uint32_t bits{m_operation_pool.RequiredWorkBits(op, CybouState{})};
    std::optional<uint64_t> nonce;
    {
        std::lock_guard lock(m_mutex);
        if (const auto solved = m_solved_work.find(*op_id); solved != m_solved_work.end()) nonce = solved->second;
        const auto loaded = m_store.LoadState();
        if (loaded && loaded.state) bits = m_operation_pool.RequiredWorkBits(op, *loaded.state);
    }
    if (!nonce || !CheckOperationWork(m_network_binding, *op_id, *nonce, bits)) {
        nonce = SolveOperationWork(m_network_binding, *op_id, bits);
        if (!nonce) return std::nullopt;
        std::lock_guard lock(m_mutex);
        if (m_solved_work.size() >= 1024) m_solved_work.erase(m_solved_work.begin());
        m_solved_work[*op_id] = *nonce;
    }
    return nonce;
}

bool CybouNodeRuntime::IsPublicationActive(const cybou::Hash256& publication_id) const
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    return loaded && loaded.state && loaded.state->publications.contains(publication_id);
}

bool CybouNodeRuntime::IsStorageLeaseActive(const cybou::Hash256& publication_id) const
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (!loaded || !loaded.state || !loaded.state->publications.contains(publication_id)) return false;
    const auto lease = loaded.state->leases.find(publication_id);
    const auto period = loaded.state->settlement.next_period;
    return lease != loaded.state->leases.end() && lease->second.first_period <= period &&
        period < lease->second.end_period;
}

OperationSubmitResult CybouNodeRuntime::SubmitOperation(ProtocolOperation op)
{
    const auto nonce = PrepareOperationWork(op);
    if (!nonce) return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED,
        .op_id = ComputeOperationId(op).value_or(cybou::Hash256{})};
    return SubmitOperationInternal(std::move(op), *nonce, std::nullopt);
}

OperationSubmitResult CybouNodeRuntime::SubmitStorageSettlement(const uint64_t period_start_utc,
    std::vector<StorageSettlementEntry> entries)
{
    std::lock_guard lock(m_mutex);
    if (!m_poa_finalizer || !m_poa_finalizer->SignerEnabled() || m_store.PoaSafetyHalted()) {
        return {.status = OperationSubmitStatus::POA_SIGNER_UNAVAILABLE};
    }
    const auto loaded = m_store.LoadState();
    if (loaded.error != StateLoadError::NONE || !loaded.state || m_poa_finalizer->SafetyHalted()) return {};
    StorageSettlement settlement{.period = loaded.state->settlement.next_period,
        .period_start_utc = period_start_utc, .entries = std::move(entries)};
    if (!m_poa_finalizer->SignStorageSettlement(settlement)) return {};
    const ProtocolOperation operation{std::move(settlement)};
    OperationSubmitStatus status{OperationSubmitStatus::REJECTED};
    // Signed by the genesis PoA key: its own protection, no relay PoW.
    switch (m_operation_pool.Admit(operation, 0)) {
    case PoolAdmission::ACCEPTED: status = OperationSubmitStatus::ACCEPTED; break;
    case PoolAdmission::ALREADY_PENDING: status = OperationSubmitStatus::ALREADY_PENDING; break;
    case PoolAdmission::ALREADY_FINALIZED: status = OperationSubmitStatus::ALREADY_FINALIZED; break;
    case PoolAdmission::REJECTED: break;
    }
    const OperationSubmitResult result{.status = status, .op_id = ComputeOperationId(operation).value_or(cybou::Hash256{})};
    if (result.status == OperationSubmitStatus::ACCEPTED) {
        RememberOperationStatus(result.op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
    }
    return result;
}

OperationRelayEnqueueStatus CybouNodeRuntime::EnqueueRelayedOperation(
    const std::span<const unsigned char> exact_bytes, const uint64_t work_nonce, const bool allow_seen_retry,
    std::optional<std::string> source_peer)
{
    const auto operation = DeserializeProtocolOperation(exact_bytes);
    if (!operation) return OperationRelayEnqueueStatus::INVALID_OPERATION;
    const auto canonical_bytes = SerializeProtocolOperation(*operation);
    if (!canonical_bytes || !std::ranges::equal(*canonical_bytes, exact_bytes)) {
        return OperationRelayEnqueueStatus::INVALID_OPERATION;
    }
    {
        std::lock_guard lock{m_mutex};
        const auto loaded = m_store.LoadState();
        if (loaded.error != StateLoadError::NONE || !loaded.state ||
            !VerifyProtocolOperationRelayProofs(*operation, m_network_binding, loaded.state->identities)) {
            return OperationRelayEnqueueStatus::INVALID_OPERATION;
        }
        // Full candidate execution, the same path PoA uses. A node never
        // forwards an operation it could not execute itself.
        switch (m_operation_pool.Admit(*operation, work_nonce, std::move(source_peer))) {
        case PoolAdmission::ACCEPTED:
        case PoolAdmission::ALREADY_PENDING:
            break;
        case PoolAdmission::ALREADY_FINALIZED:
            return OperationRelayEnqueueStatus::DUPLICATE;
        case PoolAdmission::REJECTED:
            return OperationRelayEnqueueStatus::INVALID_OPERATION;
        }
    }
    return m_operation_relay.Enqueue(exact_bytes, work_nonce, allow_seen_retry);
}

size_t CybouNodeRuntime::CandidateOperationCount() const
{
    std::lock_guard lock{m_mutex};
    return m_operation_pool.Size();
}

std::vector<cybou::Hash256> CybouNodeRuntime::CandidateOperationIds() const
{
    std::lock_guard lock{m_mutex};
    return m_operation_pool.Ids();
}

bool CybouNodeRuntime::HasCandidateOperation(const cybou::Hash256& operation_id) const
{
    std::lock_guard lock{m_mutex};
    return m_operation_pool.Contains(operation_id);
}

void CybouNodeRuntime::RevalidateCandidates()
{
    for (const auto& dropped : m_operation_pool.Revalidate()) m_operation_relay.ForgetFinalized(dropped);
}

void CybouNodeRuntime::SetIdentitySigner(IdentitySignerRef signer)
{
    std::lock_guard lock{m_mutex};
    m_identity_signer = std::move(signer);
}

std::optional<RelayedOperation> CybouNodeRuntime::ClaimRelayedOperation()
{
    return m_operation_relay.Claim();
}

std::optional<RelayedOperation> CybouNodeRuntime::NextRelayedOperation(
    const std::function<bool(const cybou::Hash256&)>& skip) const
{
    return m_operation_relay.Peek(skip);
}

void CybouNodeRuntime::ReleaseRelayedOperation(const cybou::Hash256& operation_id)
{
    m_operation_relay.Release(operation_id);
}

bool CybouNodeRuntime::AcknowledgeRelayedOperation(const cybou::Hash256& operation_id)
{
    return m_operation_relay.Acknowledge(operation_id);
}

bool CybouNodeRuntime::HasRelayedOperation(const cybou::Hash256& operation_id) const
{
    return m_operation_relay.HasQueued(operation_id);
}

OperationStatus CybouNodeRuntime::GetOperationStatus(const cybou::Hash256& op_id) const
{
    if (op_id.IsNull()) return {};
    std::lock_guard lock(m_mutex);
    if (const auto height = m_store.GetFinalizedOperationHeight(op_id)) {
        return {.kind = OperationStatusKind::FINALIZED, .finalized_height = *height};
    }
    // Only a PoA node's own pool is local pending; an ordinary node holding a
    // candidate must keep relaying it until the finalizer acknowledges it.
    if (m_poa_finalizer && m_poa_finalizer->SignerEnabled() && m_operation_pool.Contains(op_id)) {
        return {.kind = OperationStatusKind::LOCAL_PENDING};
    }
    const auto known = m_recent_operation_status.find(op_id);
    return known != m_recent_operation_status.end() ? known->second : OperationStatus{};
}

IdentityOperationCoordinator& CybouNodeRuntime::GetIdentityOperationCoordinator(CybouKeyStore& keystore)
{
    std::lock_guard lock(m_mutex);
    // One coordinator (and nonce journal) per key store: a coordinator signs
    // with the key store it was created for, so it must never be shared.
    if (const auto it = m_identity_operation_coordinators.find(&keystore); it != m_identity_operation_coordinators.end()) {
        return *it->second;
    }
    const auto index = m_identity_operation_coordinators.size();
    const auto journal = index == 0 ? m_config.data_dir / "identity-operation.cyiop"
        : m_config.data_dir / ("identity-operation-" + std::to_string(index) + ".cyiop");
    auto& coordinator = m_identity_operation_coordinators[&keystore];
    coordinator = std::make_unique<IdentityOperationCoordinator>(*this, keystore, journal);
    return *coordinator;
}

void CybouNodeRuntime::RetryPendingIdentityOperations()
{
    std::vector<IdentityOperationCoordinator*> coordinators;
    {
        std::lock_guard lock(m_mutex);
        coordinators.reserve(m_identity_operation_coordinators.size());
        for (const auto& [keystore, coordinator] : m_identity_operation_coordinators) {
            (void)keystore;
            coordinators.push_back(coordinator.get());
        }
    }
    for (auto* coordinator : coordinators) coordinator->RetryRelayIfDue();
}

void CybouNodeRuntime::RememberOperationStatus(const cybou::Hash256& id, OperationStatus status)
{
    if (id.IsNull()) return;
    if (!m_recent_operation_status.contains(id)) m_recent_operation_status_order.push_back(id);
    const auto previous = m_recent_operation_status.find(id);
    const bool changed = previous == m_recent_operation_status.end() || previous->second.kind != status.kind ||
        previous->second.finalized_height != status.finalized_height;
    m_recent_operation_status[id] = status;
    if (m_config.event_writer && changed) {
        auto event = status.kind == OperationStatusKind::FINALIZED ? NodeEvent::operation_finalized
            : status.kind == OperationStatusKind::REJECTED_KNOWN ? NodeEvent::operation_rejected
            : status.kind == OperationStatusKind::UNKNOWN ? NodeEvent::operation_uncertain
            : NodeEvent::operation_accepted;
        m_config.event_writer->Write(event, {{"operation_id",id.GetHex()},{"height",status.finalized_height}});
    }
    while (m_recent_operation_status_order.size() > 256) {
        m_recent_operation_status.erase(m_recent_operation_status_order.front());
        m_recent_operation_status_order.pop_front();
    }
}

OperationSubmitResult CybouNodeRuntime::SubmitOperationInternal(
    ProtocolOperation op, const uint64_t work_nonce, std::optional<std::string> source_peer)
{
    const cybou::Hash256 op_id = ComputeOperationId(op).value_or(cybou::Hash256{});
    std::vector<std::pair<std::string, uint16_t>> configured_endpoints;
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return OperationSubmitResult{.status = OperationSubmitStatus::NETWORK_MISMATCH, .op_id = op_id};
        }
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_store.PoaSafetyHalted() || (m_poa_finalizer && m_poa_finalizer->SafetyHalted())) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        // Every full node executes the candidate itself before anything else.
        const auto admission = m_operation_pool.Admit(op, work_nonce, std::move(source_peer));
        if (admission == PoolAdmission::ALREADY_FINALIZED) {
            const auto height = m_store.GetFinalizedOperationHeight(op_id).value_or(0);
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = height});
            return OperationSubmitResult{.status = OperationSubmitStatus::ALREADY_FINALIZED, .op_id = op_id};
        }
        if (admission == PoolAdmission::REJECTED) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_poa_finalizer && m_poa_finalizer->SignerEnabled()) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
            return OperationSubmitResult{.status = admission == PoolAdmission::ACCEPTED ?
                OperationSubmitStatus::ACCEPTED : OperationSubmitStatus::ALREADY_PENDING, .op_id = op_id};
        }
        configured_endpoints.reserve(m_config.configured_peers.size());
        for (const auto& peer : m_config.configured_peers) {
            if (m_config.advertised_endpoint != peer.endpoint) configured_endpoints.push_back(peer.endpoint);
        }
    }
    if (m_peer_manager) {
        std::lock_guard p2p_lock(m_p2p_mutex);
        // Ordinary mesh relay: any connected relay peer executes the operation
        // itself and forwards it. There is no preferred finalizer route.
        for (const auto& endpoint : configured_endpoints)
            m_peer_manager->Connect(endpoint.first, endpoint.second);
        std::vector<std::pair<std::string, uint16_t>> accepting_candidates;
        const auto peers = m_peer_manager->Peers();
        accepting_candidates.reserve(peers.size());
        for (const auto& peer : peers) {
            {
                const auto endpoint = std::make_pair(peer.address, peer.port);
                if (std::find(accepting_candidates.begin(), accepting_candidates.end(), endpoint) ==
                    accepting_candidates.end()) accepting_candidates.push_back(endpoint);
            }
        }
        const auto submitted = m_peer_manager->SubmitOperationToAny(accepting_candidates, op, work_nonce);
        // No acknowledgment from anyone is not a rejection: nothing proved the
        // operation invalid, so the exact bytes are retained for retry.
        auto result = submitted.acknowledgment.value_or(
            OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id});
        result.delivery_uncertain = submitted.delivery_uncertain || !submitted.acknowledgment ||
            (result.status != OperationSubmitStatus::ACCEPTED && result.status != OperationSubmitStatus::ALREADY_PENDING &&
                result.status != OperationSubmitStatus::RELAY_QUEUED);
        {
            std::lock_guard lock(m_mutex);
            if (result.status == OperationSubmitStatus::ACCEPTED ||
                result.status == OperationSubmitStatus::ALREADY_PENDING ||
                result.status == OperationSubmitStatus::RELAY_QUEUED) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
            } else if (const auto finalized = m_store.GetFinalizedOperationHeight(op_id)) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                    .finalized_height = *finalized});
                result.status = OperationSubmitStatus::ALREADY_FINALIZED;
                result.delivery_uncertain = false;
            } else if (result.delivery_uncertain) {
                if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) result.status = OperationSubmitStatus::REJECTED;
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::UNKNOWN});
            } else {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
        }
        return result;
    }
    return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
}

void CybouNodeRuntime::EmitFinalizedEvents(const FinalizedBlock& block, bool produced)
{
    EventFields fields{{"height",block.block.height},{"block_id",ComputeBlockId(block.block).GetHex()},
        {"state_root",block.block.resulting_state_root.GetHex()}};
    if (m_config.event_writer) {
        m_config.event_writer->Write(produced ? NodeEvent::block_produced : NodeEvent::block_verified, fields);
        m_config.event_writer->Write(NodeEvent::block_finalized, fields);
    }
    for (const auto& op : block.block.operations) if (const auto id = ComputeOperationId(op)) {
        // Finalization is the only canonical answer. Once known, clear any
        // lingering relay work and collapse local status to FINALIZED.
        m_operation_relay.ForgetFinalized(*id);
        if (const auto* revoke = std::get_if<AuthorizedRevokePublication>(&op); revoke && m_finalized_chunk_store) {
            // Finalized revocation: the author deleted the object; drop its replicas now.
            (void)m_finalized_chunk_store->PurgePublication(revoke->revoke.publication_id);
        }
        RememberOperationStatus(*id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = block.block.height});
          if (m_config.event_writer) std::visit([&](const auto& value) {
              EventFields operation{{"operation_id",id->GetHex()},{"height",block.block.height}};
              if constexpr (requires { value.authorization.account_id; value.authorization.nonce; }) {
                  operation.emplace("account_id",value.authorization.account_id.Value().GetHex());
                  operation.emplace("nonce",value.authorization.nonce);
              } else if constexpr (requires { value.account_id; value.nonce; }) {
                  operation.emplace("account_id",value.account_id.Value().GetHex());
                  operation.emplace("nonce",value.nonce);
              }
              m_config.event_writer->Write(NodeEvent::operation_finalized,operation);
          },op);
    }
}

std::optional<FinalizedBlock> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::lock_guard lock(m_mutex);
    if (m_production_status == BlockProductionStatus::SAFETY_HALT) return std::nullopt;
    m_production_status = BlockProductionStatus::RETRY;
    if (!m_poa_finalizer || !m_poa_finalizer->SignerEnabled()) {
        m_production_status = BlockProductionStatus::SIGNER_UNAVAILABLE;
        return std::nullopt;
    }
    if (m_store.PoaSafetyHalted() || m_poa_finalizer->SafetyHalted()) {
        m_production_status = BlockProductionStatus::SAFETY_HALT;
        return std::nullopt;
    }
    try {
        const auto loaded = m_store.LoadState();
        if (loaded.error != StateLoadError::NONE || !loaded.state) {
            m_production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        const auto head = m_store.GetFinalizedHead();
        if (!head || head->height == std::numeric_limits<uint64_t>::max()) return std::nullopt;
        if (m_poa_finalizer->CheckCanonicalTip(head->height, head->block_id) != PoaJournalStatus::NONE) {
            m_production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        if (m_production_candidate && m_production_candidate->height <= head->height) {
            // A concurrent verified commit may have completed the same intent.
            m_production_candidate.reset();
            m_production_finalized.reset();
        }
        if (!m_production_candidate) {
            const auto operations = m_operation_pool.Snapshot();
            const auto root = m_store.ComputeCandidateStateRoot(operations, head->height + 1);
            if (!root) { RevalidateCandidates(); return std::nullopt; }
            // Snapshot() preserves the node's locally validated candidate order;
            // PoA finalization never imports an external mempool ordering.
            CybouBlock candidate{.parent_block_id = head->block_id,
                .height = head->height + 1, .operations = operations, .resulting_state_root = *root};
            const auto encoded = SerializeBlock(candidate);
            if (!encoded || encoded->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES -
                    POA_FINALITY_CERTIFICATE_SIZE - 8) return std::nullopt;
            m_production_candidate = std::move(candidate);
        }
        if (!m_production_finalized) {
            const auto signing = m_poa_finalizer->SignFinality(head->height, head->block_id, *m_production_candidate);
            if (!signing.certificate) {
                if (signing.status == PoaSigningStatus::JOURNAL_REJECTED)
                    m_production_status = BlockProductionStatus::SAFETY_HALT;
                return std::nullopt;
            }
            m_production_finalized = FinalizedBlock{.block = *m_production_candidate, .certificate = *signing.certificate};
        }
        const auto bytes = SerializeFinalizedBlock(*m_production_finalized);
        if (!bytes || bytes->size() > MAX_FINALIZER_SERIALIZED_BLOCK_BYTES) {
            m_production_status = BlockProductionStatus::SAFETY_HALT;
            return std::nullopt;
        }
        if (!m_store.CommitFinalizedBlock(*m_production_finalized, sync)) return std::nullopt;
        auto finalized = std::move(*m_production_finalized);
        m_production_candidate.reset();
        m_production_finalized.reset();
        m_production_status = BlockProductionStatus::PRODUCED;
        EmitFinalizedEvents(finalized, true);
        RevalidateCandidates();
        return finalized;
    } catch (...) {
        // Journal failures are never retried as ordinary transport/storage failures.
        if (m_poa_finalizer->SafetyHalted()) m_production_status = BlockProductionStatus::SAFETY_HALT;
        return std::nullopt;
    }
}

BlockProductionStatus CybouNodeRuntime::LastBlockProductionStatus() const
{
    std::lock_guard lock(m_mutex);
    return m_production_status;
}

bool CybouNodeRuntime::EnablePoaSigner(std::shared_ptr<PoaSigner> signer)
{
    if (!signer) return false;
    bool enabled{false};
    {
        std::lock_guard lock(m_mutex);
        if (!m_poa_finalizer) m_poa_finalizer = std::make_unique<PoaFinalizer>(m_store.GetDatabase(), m_store.GetNetworkBinding(),
            m_store.GetNetworkGenesis().GetGenesisAnchor(), m_store.GetNetworkGenesis().GetPoaPublicKey());
        if (m_production_status == BlockProductionStatus::SAFETY_HALT || m_store.PoaSafetyHalted()) return false;
        enabled = m_poa_finalizer->EnableSigner(std::move(signer));
    }
    return enabled;
}

void CybouNodeRuntime::DisablePoaSigner()
{
    {
        std::lock_guard lock(m_mutex);
        if (m_poa_finalizer) m_poa_finalizer->DisableSigner();
    }

}

bool CybouNodeRuntime::IsPoaSignerActive() const
{
    std::lock_guard lock(m_mutex);
    return m_poa_finalizer && m_poa_finalizer->SignerEnabled();
}

BlockTransitionResult CybouNodeRuntime::CommitBlock(const FinalizedBlock& block, const bool sync)
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        return BlockTransitionResult{.error = BlockTransitionError::NETWORK_MISMATCH};
    }
    if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
        return BlockTransitionResult{.error = BlockTransitionError::STATE_NOT_INITIALIZED};
    }
    const auto result = m_store.CommitFinalizedBlock(block, sync);
    if (result) {
        EmitFinalizedEvents(block, false);
        RevalidateCandidates();
    }
    return result;
}

std::optional<FinalizedBlock> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetBlockAtHeight(height);
}

FinalizedOperationLookupResult CybouNodeRuntime::FindFinalizedOperation(const cybou::Hash256& op_id) const
{
    FinalizedOperationLookupResult result;
    const auto status = GetStatus();
    if (!status.is_initialized || op_id.IsNull()) return result;
    result.status = FinalizedOperationLookupStatus::NOT_FOUND;
    cybou::Hash256 previous_id = m_config.network_genesis.GetGenesisAnchor();
    for (uint64_t height = 1; height <= status.finalized_height; ++height) {
        const auto finalized = GetBlockAtHeight(height);
        if (!finalized) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        const auto block_id = ComputeBlockId(finalized->block);
        // Lookup is intentionally strict: any gap or certificate mismatch means
        // the local history cannot be used as verified evidence for this answer.
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_binding != status.network_binding ||
            finalized->certificate.height != height ||
            finalized->certificate.block_id != block_id) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        for (size_t index = 0; index < finalized->block.operations.size(); ++index) {
            const auto candidate = ComputeOperationId(finalized->block.operations[index]);
            if (!candidate) {
                result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
                return result;
            }
            if (*candidate == op_id) {
                result.status = FinalizedOperationLookupStatus::FOUND;
                result.scanned_height = height;
                result.height = height;
                result.operation_index = static_cast<uint32_t>(index);
                result.block_id = block_id;
                return result;
            }
        }
        previous_id = block_id;
        result.scanned_height = height;
        if (height == status.finalized_height) break;
    }
    return result;
}

std::optional<RootPublication> CybouNodeRuntime::FindFinalizedRootPublication(const cybou::Hash256& op_id) const
{
    const auto location = FindFinalizedOperation(op_id);
    if (location.status != FinalizedOperationLookupStatus::FOUND) return std::nullopt;
    const auto finalized = GetBlockAtHeight(location.height);
    if (!finalized || location.operation_index >= finalized->block.operations.size() ||
        ComputeBlockId(finalized->block) != location.block_id) return std::nullopt;
    const auto& operation = finalized->block.operations[location.operation_index];
    const auto operation_id = ComputeOperationId(operation);
    if (!operation_id || *operation_id != op_id) return std::nullopt;
    const auto* publication = std::get_if<AuthorizedRootPublication>(&operation);
    if (!publication) return std::nullopt;
    // A revoked publication authorizes nothing any more (DEC-271): it left the register.
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (!loaded || !loaded.state || !loaded.state->publications.contains(op_id)) return std::nullopt;
    return publication->publication;
}

IdentityKemPackageLookupResult CybouNodeRuntime::FindIdentityKemPackage(
    const AccountId& account_id, const uint64_t key_epoch) const
{
    std::lock_guard lock(m_mutex);
    IdentityKemPackageLookupResult result;
    const auto loaded = m_store.LoadState();
    const auto finalized_height = m_store.GetFinalizedHeight();
    const auto state_root = m_store.GetStateRoot();
    if (!loaded || !loaded.state || !finalized_height || !state_root || account_id.IsNull()) return result;
    const auto* identity = loaded.state->identities.Find(account_id);
    if (!identity) {
        result.status = IdentityKemPackageLookupStatus::ACCOUNT_NOT_FOUND;
        return result;
    }
    result.finalized_height = *finalized_height;
    result.state_root = *state_root;
    if (key_epoch > identity->key_epoch) {
        result.status = IdentityKemPackageLookupStatus::KEY_EPOCH_UNAVAILABLE;
        return result;
    }

    bool found{false};
    const auto account_bytes = account_id.Value();
    cybou::Hash256 previous_id = m_config.network_genesis.GetGenesisAnchor();
    for (uint64_t height = 1; height <= *finalized_height; ++height) {
        const auto finalized = m_store.GetBlockAtHeight(height);
        if (!finalized) return result;
        const auto block_id = ComputeBlockId(finalized->block);
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_binding != m_network_binding ||
            finalized->certificate.height != height ||
            finalized->certificate.block_id != block_id) return result;
        for (size_t index = 0; index < finalized->block.operations.size(); ++index) {
            const auto& operation = finalized->block.operations[index];
            if (!ComputeOperationId(operation)) return result;
            const IdentityKemPackage* package{nullptr};
            AccountId published_account;
            uint64_t published_epoch{0};
            if (const auto* create = std::get_if<AccountCreateOp>(&operation)) {
                published_account = create->account_id;
                package = &create->kem_package;
            } else if (const auto* rotate = std::get_if<IdentityRotate>(&operation)) {
                published_account = rotate->account_id;
                published_epoch = rotate->key_epoch;
                package = &rotate->new_kem_package;
            }
            if (!package || published_account != account_id || published_epoch != key_epoch) continue;
            if (found) return result;
            const auto commitment = ComputeIdentityKemPackageCommitment(
                std::span<const unsigned char, 32>{m_network_binding.begin(), 32},
                std::span<const unsigned char, 32>{account_bytes.begin(), 32}, key_epoch, *package);
            if (!commitment) return result;
            if (key_epoch == identity->key_epoch && *commitment != identity->kem_package_id) {
                result.status = IdentityKemPackageLookupStatus::HISTORY_UNAVAILABLE;
                return result;
            }
            found = true;
            result.package = *package;
            result.package_id = *commitment;
            result.key_epoch = key_epoch;
            result.operation_height = height;
            result.operation_index = static_cast<uint32_t>(index);
            result.block_id = block_id;
        }
        previous_id = block_id;
        if (height == *finalized_height) break;
    }
    result.status = found ? IdentityKemPackageLookupStatus::FOUND : IdentityKemPackageLookupStatus::NOT_FOUND;
    return result;
}

void CybouNodeRuntime::SchedulePeerRetry(
    const std::pair<std::string, uint16_t>& endpoint, const PeerFailureClass failure)
{
    auto& retry = m_peer_retry_after[endpoint];
    const auto now = std::chrono::steady_clock::now();
    const auto backoff = [](uint32_t failures, const uint32_t base_seconds, const uint32_t max_seconds) {
        uint32_t delay = base_seconds;
        while (failures > 1 && delay < max_seconds) {
            delay = std::min(max_seconds, delay * 2);
            --failures;
        }
        return std::chrono::seconds{delay};
    };

    switch (failure) {
    case PeerFailureClass::TEMPORARY:
        retry.temporary_failures = std::min<uint32_t>(retry.temporary_failures + 1, 16);
        retry.retry_after = now + backoff(retry.temporary_failures, 5, 300);
        break;
    case PeerFailureClass::PROTOCOL:
        // Protocol mismatches back off far more aggressively than transient
        // reachability to avoid hammering obviously incompatible peers.
        retry.protocol_failures = std::min<uint32_t>(retry.protocol_failures + 1, 16);
        retry.retry_after = now + backoff(retry.protocol_failures, 1800, 86400);
        break;
    case PeerFailureClass::WRONG_NETWORK:
        retry.retry_after = now + std::chrono::hours{24};
        break;
    }
}

SyncPeerResult CybouNodeRuntime::SyncFromConfiguredPeer(const uint64_t max_blocks)
{
    (void)m_finalized_chunk_store->RetryPendingPurges();
    if (!m_peer_manager) return {};
    RetryPendingIdentityOperations();
    std::lock_guard p2p_lock(m_p2p_mutex);

    const auto local_status = GetStatus();
    if (local_status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH) {
        return SyncPeerResult{.status = SyncPeerStatus::NETWORK_MISMATCH};
    }
    if (local_status.runtime_state == NodeRuntimeState::CORRUPT || !local_status.is_initialized) {
        return SyncPeerResult{.status = SyncPeerStatus::PROTOCOL_ERROR};
    }

    auto explicit_endpoints = GetConfiguredPeerEndpoints();
    m_peer_manager->SetExplicitEndpoints(explicit_endpoints);
    const auto maintenance_now = std::chrono::steady_clock::now();
    const bool have_connected_peers = m_peer_manager->ConnectedCount() != 0;
    if (have_connected_peers && maintenance_now >= m_next_peer_discovery) {
        m_next_peer_discovery = maintenance_now + std::chrono::seconds{60};
        m_peer_manager->DiscoverPeers(1);
    } else if (have_connected_peers && maintenance_now >= m_next_peer_ping) {
        m_next_peer_ping = maintenance_now + std::chrono::seconds{15};
        m_peer_manager->PingSome(1);
    }

    const auto targets = GetPeerEndpointsForGossip();
    const auto connected_before_dial = m_peer_manager->Peers();
    if (connected_before_dial.size() < p2p::MAX_OUTBOUND_PEERS) {
        const auto now = std::chrono::steady_clock::now();
        const auto candidate = std::find_if(targets.begin(), targets.end(), [&](const auto& endpoint) {
            const bool connected = std::any_of(connected_before_dial.begin(), connected_before_dial.end(),
                [&](const p2p::PeerInfo& peer) { return peer.address == endpoint.first && peer.port == endpoint.second; });
            const auto retry = m_peer_retry_after.find(endpoint);
            return !connected && (retry == m_peer_retry_after.end() || now >= retry->second.retry_after);
        });
        if (candidate != targets.end()) {
            if (m_peer_manager->Connect(candidate->first, candidate->second)) {
                m_peer_retry_after.erase(*candidate);
            } else {
                switch (m_peer_manager->LastConnectStatus()) {
                case p2p::PeerConnectStatus::UNAVAILABLE:
                    SchedulePeerRetry(*candidate, PeerFailureClass::TEMPORARY);
                    break;
                case p2p::PeerConnectStatus::WRONG_NETWORK:
                    SchedulePeerRetry(*candidate, PeerFailureClass::WRONG_NETWORK);
                    break;
                case p2p::PeerConnectStatus::HANDSHAKE_FAILED:
                    SchedulePeerRetry(*candidate, PeerFailureClass::PROTOCOL);
                    break;
                default:
                    break;
                }
            }
        }
    }

    auto peers = m_peer_manager->Peers();
    if (peers.empty()) {
        return SyncPeerResult{.status = SyncPeerStatus::CONNECTION_FAILED};
    }
    const auto route_rank = [&](const p2p::PeerInfo& peer) {
        const auto endpoint = std::make_pair(peer.address, peer.port);
        const auto found = std::find(explicit_endpoints.begin(), explicit_endpoints.end(), endpoint);
        return found == explicit_endpoints.end() ? explicit_endpoints.size() :
            static_cast<size_t>(found - explicit_endpoints.begin());
    };
    // HELLO heights are connection-time snapshots. Prefer configured routes so
    // mutually lagging discovered providers cannot eclipse the bootstrap peer.
    // An unavailable or up-to-date configured peer still falls through below.
    std::sort(peers.begin(), peers.end(), [&](const p2p::PeerInfo& left, const p2p::PeerInfo& right) {
        if (route_rank(left) != route_rank(right)) return route_rank(left) < route_rank(right);
        if (left.hello.finalized_height != right.hello.finalized_height) {
            return left.hello.finalized_height > right.hello.finalized_height;
        }
        if (left.address != right.address) return left.address < right.address;
        return left.port < right.port;
    });

    SyncPeerResult result{.status = SyncPeerStatus::CONNECTION_FAILED};
    bool any_peer_up_to_date{false};
    bool all_peers_caught_up{!peers.empty()};
    for (const auto& peer : peers) {
        const auto attempt = m_peer_manager->SyncFromPeer(peer.address, peer.port, max_blocks-result.blocks_applied);
        // Completion covers known reachable peers only; it conveys no consensus trust.
        if (!attempt.caught_up_with_known_peers) all_peers_caught_up = false;
        if (attempt.status == SyncPeerStatus::BLOCKS_APPLIED) {
            result.status=SyncPeerStatus::BLOCKS_APPLIED;
            result.blocks_applied+=attempt.blocks_applied;
            if (result.blocks_applied>=max_blocks) { all_peers_caught_up = false; break; }
            // Partial progress from a slow route must not hide a fresher peer.
            continue;
        }
        // HELLO height is only a snapshot from connection time. A peer that
        // reports UP_TO_DATE may have stopped advancing while another
        // connected peer has newer blocks, so keep checking the whole set.
        // Peer protocol failures are isolated to that session by PeerManager;
        // they must not become a fatal network-service result.
        if (attempt.status == SyncPeerStatus::UP_TO_DATE) {
            any_peer_up_to_date = true;
            m_peer_retry_after.erase({peer.address, peer.port});
        } else if (attempt.status == SyncPeerStatus::CONNECTION_FAILED) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::TEMPORARY);
        } else if (attempt.status == SyncPeerStatus::NETWORK_MISMATCH) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::WRONG_NETWORK);
        } else if (attempt.status == SyncPeerStatus::PROTOCOL_ERROR) {
            SchedulePeerRetry({peer.address, peer.port}, PeerFailureClass::PROTOCOL);
        }
    }
    result.caught_up_with_known_peers = all_peers_caught_up;
    if (result.blocks_applied==0 && any_peer_up_to_date) result.status = SyncPeerStatus::UP_TO_DATE;
    m_peer_manager->PollOperationRelays();
    m_peer_manager->FanoutFinalizedBlocks();
    return result;
}

NodeDiagnosticsSnapshot CybouNodeRuntime::GetDiagnostics() const
{
    const auto status = GetStatus();
    NodeDiagnosticsSnapshot snapshot;
    snapshot.network_binding = status.network_binding.GetHex();
    snapshot.node_type = "Full Node";
    snapshot.poa_signer_active = status.poa_signer_active;
    snapshot.height = status.finalized_height;
    snapshot.tip = status.finalized_tip.GetHex();
    snapshot.state_root = status.state_root.GetHex();
    snapshot.initialized = status.is_initialized;
    snapshot.safety_halted = status.poa_safety_halted;
    // Never hold state and peer locks together (peer I/O can call state methods).
    {
        std::lock_guard lock{m_mutex};
        snapshot.operations.reserve(m_recent_operation_status_order.size());
        for (const auto& id : m_recent_operation_status_order) {
            const auto& op = m_recent_operation_status.at(id);
            snapshot.operations.push_back({id.GetHex(), static_cast<std::uint32_t>(op.kind), op.finalized_height});
        }
    }
    {
        std::lock_guard lock{m_p2p_mutex};
        if (m_peer_manager) {
            const auto peers = m_peer_manager->Peers();
            snapshot.peers.reserve(peers.size());
            for (const auto& peer : peers) {
                std::string provider;
                if (peer.storage_id) {
                    static constexpr char HEX[] = "0123456789abcdef";
                    provider.reserve(peer.storage_id->size() * 2);
                    for (auto b : *peer.storage_id) {
                        provider += HEX[b >> 4];
                        provider += HEX[b & 15];
                    }
                }
                snapshot.peers.push_back({peer.address + ":" + std::to_string(peer.port),
                    peer.hello.finalized_height, provider});
            }
        }
    }
    if (m_finalized_chunk_store) {
        snapshot.storage_used = m_finalized_chunk_store->UsedBytes();
        snapshot.storage_capacity = m_finalized_chunk_store->CapacityBytes();
    }
    if (m_chunk_blob_store) {
        snapshot.local_storage_used = m_chunk_blob_store->UsedBytes();
        snapshot.local_storage_capacity = m_config.storage_capacity_bytes.value_or(0);
    }
    return snapshot;
}

size_t CybouNodeRuntime::ConnectedPeerCount() const
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    return m_peer_manager ? m_peer_manager->ConnectedCount() : 0;
}

std::optional<std::array<unsigned char, 32>> CybouNodeRuntime::PinnedSpki(
    const std::string& address, const uint16_t port) const
{
    std::lock_guard lock(m_mutex);
    for (const auto& peer : m_config.configured_peers) {
        if (peer.endpoint.first == address && peer.endpoint.second == port) return peer.tls_spki_sha256;
    }
    return std::nullopt;
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetPeerEndpointsForGossip() const
{
    std::lock_guard lock(m_mutex);
    constexpr size_t MAX_GOSSIP_TARGETS{32};
    std::vector<std::pair<std::string, uint16_t>> result;
    result.reserve(MAX_GOSSIP_TARGETS);
    for (const auto& peer : m_config.configured_peers) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (m_config.advertised_endpoint == peer.endpoint) continue;
        if (std::find(result.begin(), result.end(), peer.endpoint) == result.end()) result.push_back(peer.endpoint);
    }
    for (const auto& ep : m_discovered_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (std::find(result.begin(), result.end(), ep) == result.end()) result.push_back(ep);
    }
    return result;
}

void CybouNodeRuntime::SetConfiguredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    // Retain release-pinned configured entries when replacing operator routes.
    std::erase_if(m_config.configured_peers, [](const ConfiguredPeer& peer) { return !peer.tls_spki_sha256; });
    for (const auto& [host, port] : endpoints) {
        if (port == 0 || m_config.configured_peers.size() >= 32) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        const Endpoint endpoint{addr.to_string(), port};
        if (m_config.advertised_endpoint == endpoint ||
            std::any_of(m_config.configured_peers.begin(), m_config.configured_peers.end(),
                [&](const ConfiguredPeer& peer) { return peer.endpoint == endpoint; })) continue;
        m_config.configured_peers.push_back({endpoint, std::nullopt});
    }
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetConfiguredPeerEndpoints() const
{
    std::lock_guard lock(m_mutex);
    std::vector<Endpoint> result;
    result.reserve(m_config.configured_peers.size());
    for (const auto& peer : m_config.configured_peers)
        if (m_config.advertised_endpoint != peer.endpoint) result.push_back(peer.endpoint);
    return result;
}

void CybouNodeRuntime::AddDiscoveredPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    constexpr size_t MAX_DISCOVERED_PEER_ENDPOINTS{256};
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (!IsConnectableDiscoveredAddress(addr, m_config.advertised_endpoint)) continue;
        const auto canonical = std::make_pair(addr.to_string(), port);
        if (m_config.advertised_endpoint && canonical == *m_config.advertised_endpoint) continue;
        if (std::any_of(m_config.configured_peers.begin(), m_config.configured_peers.end(),
                [&](const ConfiguredPeer& peer) { return peer.endpoint == canonical; })) continue;
        if (m_discovered_peer_endpoints.size() >= MAX_DISCOVERED_PEER_ENDPOINTS) break;
        m_discovered_peer_endpoints.emplace(canonical);
    }
}

} // namespace cybou
