// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/node_runtime.h>
#include <cybou/identity_operation_coordinator.h>
#include <cybou/keystore.h>
#include <cybou/p2p/session.h>
#include <cybou/identity_crypto.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/p2p/peer_manager.h>

#include <boost/asio/ip/address.hpp>
#include <openssl/rand.h>

#include <algorithm>
#include <fstream>
#include <limits>

namespace cybou {

namespace {

// Discovered routing hints are untrusted. Reject address scopes this node has
// no business dialing: unspecified, multicast, and (unless the local CYP2
// listener lives in the same scope) loopback and link-local targets. Without a
// known local listener the policy stays permissive for DEV tooling.
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

bool IsConnectableDiscoveredAddress(
    const boost::asio::ip::address& addr,
    const std::optional<std::pair<std::string, uint16_t>>& local_p2p_endpoint)
{
    auto is_link_local = [](const boost::asio::ip::address& a) {
        if (a.is_v4()) return (a.to_v4().to_uint() & 0xFFFF0000U) == 0xA9FE0000U;
        const auto bytes = a.to_v6().to_bytes();
        return bytes[0] == 0xFEU && (bytes[1] & 0xC0U) == 0x80U;
    };
    if (addr.is_unspecified() || addr.is_multicast()) return false;
    if (IsPrivateAddress(addr) && local_p2p_endpoint) {
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        if (!ec && !IsPrivateAddress(local) && !local.is_loopback() && !is_link_local(local) &&
            !local.is_unspecified() && !local.is_multicast()) {
            return false;
        }
    }
    if (addr.is_loopback()) {
        if (!local_p2p_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        return !ec && local.is_loopback();
    }
    if (is_link_local(addr)) {
        if (!local_p2p_endpoint) return true;
        boost::system::error_code ec;
        const auto local = boost::asio::ip::make_address(local_p2p_endpoint->first, ec);
        return !ec && is_link_local(local);
    }
    return true;
}

} // namespace

namespace {
/** 32 random bytes kept beside provider data (0600); in memory for memory-only runtimes. */
std::optional<std::array<unsigned char, 32>> LoadOrCreateProviderSecret(const std::filesystem::path& path)
{
    std::array<unsigned char, 32> secret{};
    if (!path.empty() && std::filesystem::exists(path)) {
        std::ifstream in(path, std::ios::binary);
        in.read(reinterpret_cast<char*>(secret.data()), secret.size());
        if (!in || in.peek() != std::char_traits<char>::eof()) return std::nullopt;
        return secret;
    }
    if (RAND_bytes(secret.data(), static_cast<int>(secret.size())) != 1) return std::nullopt;
    if (path.empty()) return secret;
    std::filesystem::create_directories(path.parent_path());
    const auto temp = path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(secret.data()), secret.size());
        if (!out) return std::nullopt;
    }
    std::filesystem::permissions(temp, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    std::filesystem::rename(temp, path);
    return secret;
}
} // namespace

CybouNodeRuntime::CybouNodeRuntime(NodeRuntimeConfig config)
    : m_config{std::move(config)},
      m_network_id{NetworkId(m_config.network_definition)},
      m_db{std::make_unique<KVStore>(KVStoreOptions{
          .path = m_config.data_dir,
          .cache_bytes = m_config.db_cache_bytes,
          .memory_only = m_config.memory_only,
          .wipe_data = m_config.wipe_data,
      })},
      m_store{*m_db, m_config.network_definition},
      m_submit_endpoint{m_config.submit_endpoint}
{
    std::filesystem::path storage_path;
    if (!m_config.memory_only) {
        storage_path = std::filesystem::path{m_config.data_dir.string() + ".chunks"};
    }
    m_chunk_blob_store = std::make_unique<ChunkBlobStore>(
        m_config.memory_only ? std::filesystem::path{} : storage_path / "chunks",
        m_config.memory_only, m_config.wipe_data);
    m_chunk_retention = std::make_unique<ChunkRetentionRegistry>(
        m_config.memory_only ? std::filesystem::path{} : storage_path / "retention",
        m_config.memory_only, m_config.wipe_data);
    if (m_config.storage_enabled) {
        if (m_config.storage_capacity_bytes == 0) {
            throw std::invalid_argument("storage provider requires a positive capacity");
        }
        m_finalized_chunk_store = std::make_unique<FinalizedChunkStore>(*m_chunk_blob_store, storage_path,
            std::span<const unsigned char, 32>{m_network_id.begin(), 32},
            m_config.storage_capacity_bytes, m_config.wipe_data);
        // The provider key is this node's stable storage identity across restarts.
        m_provider_secret = LoadOrCreateProviderSecret(m_config.memory_only ? std::filesystem::path{} :
            storage_path / "provider.key");
        if (!m_provider_secret) throw std::runtime_error("cannot load or create the storage provider key");
        const auto proof = SignProviderProof(p2p::ProviderProofMessage(m_network_id, 1, 2));
        m_provider_id = proof ? p2p::VerifyProviderProof(*proof, p2p::ProviderProofMessage(m_network_id, 1, 2))
                              : std::nullopt;
        if (!m_provider_id) throw std::runtime_error("storage provider key is invalid");
    }
    if (m_config.validator_private_key.has_value()) {
        m_authority_node = std::make_unique<CybouAuthorityNode>(
            m_store, *m_config.validator_private_key);
    }
    if (m_config.p2p_endpoint) m_peer_manager = std::make_unique<p2p::PeerManager>(*this);
}

CybouNodeRuntime::~CybouNodeRuntime()
{
    if (m_provider_secret) crypto::CleanseMemory(m_provider_secret->data(), m_provider_secret->size());
}

std::optional<std::array<unsigned char, 32>> CybouNodeRuntime::LocalProviderId() const { return m_provider_id; }

ChunkRetentionRegistry::CollectResult CybouNodeRuntime::CollectChunkGarbage(const std::uint64_t cache_budget_bytes,
    const std::uint64_t now_ms, const std::size_t max_removals)
{
    // A freshly cached or released blob may be in active use by a download.
    constexpr std::uint64_t GRACE_MS{10 * 60 * 1000};
    return m_chunk_retention->Collect(*m_chunk_blob_store, cache_budget_bytes, now_ms, GRACE_MS, max_removals,
        [&](const ChunkId& id) {
            return m_finalized_chunk_store ? m_finalized_chunk_store->RemoveUnlessAdmitted(id)
                                           : m_chunk_blob_store->Remove(id);
        });
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::SignProviderProof(
    const std::span<const unsigned char> message) const
{
    if (!m_provider_secret) return std::nullopt;
    const auto key = DeriveIdentityPublicKey(*m_provider_secret, IdentityKeyPurpose::STORAGE_PROVIDER);
    const auto signature = SignIdentityMessage(*m_provider_secret, IdentityKeyPurpose::STORAGE_PROVIDER, message);
    if (!key || !signature) return std::nullopt;
    std::vector<unsigned char> proof(key->ed25519.begin(), key->ed25519.end());
    proof.insert(proof.end(), key->ml_dsa.begin(), key->ml_dsa.end());
    proof.insert(proof.end(), signature->ed25519.begin(), signature->ed25519.end());
    proof.insert(proof.end(), signature->ml_dsa.begin(), signature->ml_dsa.end());
    return proof;
}

ChunkAdmissionResult CybouNodeRuntime::PutFinalizedChunk(
    const uint256& publication_operation_id, const ChunkId& chunk_id,
    const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    if (!m_finalized_chunk_store) return {ChunkAdmissionStatus::STORAGE_ERROR};
    return m_finalized_chunk_store->PutChunk(publication_operation_id, chunk_id, stored_bytes, proof,
        [this](const uint256& operation_id) { return FindFinalizedRootPublication(operation_id); });
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetFinalizedChunk(const ChunkId& chunk_id) const
{
    return m_finalized_chunk_store ? m_finalized_chunk_store->GetChunk(chunk_id) : std::nullopt;
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetFinalizedChunkAuthorizationProof(
    const uint256& publication_operation_id, const ChunkId& chunk_id) const
{
    if (!m_finalized_chunk_store) return std::nullopt;
    return m_finalized_chunk_store->GetChunkAuthorizationProof(publication_operation_id, chunk_id,
        [this](const uint256& operation_id) { return FindFinalizedRootPublication(operation_id); });
}

bool CybouNodeRuntime::HasFinalizedChunk(const ChunkId& chunk_id) const
{
    return m_finalized_chunk_store && m_finalized_chunk_store->HasChunk(chunk_id);
}

std::vector<CybouNodeRuntime::StoragePeer> CybouNodeRuntime::StoragePeerEndpoints() const
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    std::vector<StoragePeer> endpoints;
    if (!m_peer_manager) return endpoints;
    for (const auto& peer : m_peer_manager->StoragePeers()) {
        if (peer.provider_id) endpoints.push_back({peer.address, peer.port, *peer.provider_id});
    }
    return endpoints;
}

std::optional<ChunkAdmissionResult> CybouNodeRuntime::PutChunkToStoragePeer(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& provider_id, const uint256& publication_operation_id,
    const ChunkId& chunk_id, const std::span<const unsigned char> stored_bytes, const ChunkAuthorizationProof& proof)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->PutAuthorizedChunk(address, port, provider_id, publication_operation_id, chunk_id,
        stored_bytes, proof);
}

std::optional<std::vector<unsigned char>> CybouNodeRuntime::GetChunkFromStoragePeer(const std::string& address,
    const uint16_t port, const std::array<unsigned char, 32>& provider_id, const ChunkId& chunk_id)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->GetChunkById(address, port, provider_id, chunk_id);
}

std::optional<ChunkAuthorizationProof> CybouNodeRuntime::GetChunkAuthorizationProofFromStoragePeer(
    const std::string& address, const uint16_t port, const uint256& publication_operation_id,
    const ChunkId& chunk_id)
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    if (!m_peer_manager) return std::nullopt;
    return m_peer_manager->GetChunkAuthorizationProof(address, port, publication_operation_id, chunk_id);
}

bool CybouNodeRuntime::InitializeGenesis(const CybouState& genesis, const bool sync)
{
    std::lock_guard lock(m_mutex);
    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NONE && loaded.state.has_value()) {
        return true;
    }
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
    status.network_id = m_network_id;
    status.is_authority = (m_authority_node != nullptr);

    const auto loaded = m_store.LoadState();
    if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
        status.runtime_state = NodeRuntimeState::NETWORK_MISMATCH;
    } else if (loaded.error == StateLoadError::CORRUPT || loaded.error == StateLoadError::INVALID_NETWORK_DEFINITION) {
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
    status.poa_safety_halted = m_store.PoaSafetyHalted() ||
        (m_authority_node && m_authority_node->SafetyHalted());
    if (status.poa_safety_halted) status.runtime_state = NodeRuntimeState::SAFETY_HALTED;
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

std::optional<uint256> CybouNodeRuntime::GetFinalizedTip() const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetFinalizedTip();
}

std::optional<uint256> CybouNodeRuntime::GetStateRoot() const
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

OperationSubmitResult CybouNodeRuntime::SubmitOperation(ProtocolOperation op)
{
    return SubmitOperationInternal(std::move(op), std::nullopt);
}

OperationSubmitResult CybouNodeRuntime::SubmitPeerOperation(ProtocolOperation op, std::string source_peer)
{
    return SubmitOperationInternal(std::move(op), std::move(source_peer));
}

OperationStatus CybouNodeRuntime::GetOperationStatus(const uint256& op_id) const
{
    if (op_id.IsNull()) return {};
    std::lock_guard lock(m_mutex);
    if (const auto height = m_store.GetFinalizedOperationHeight(op_id)) {
        return {.kind = OperationStatusKind::FINALIZED, .finalized_height = *height};
    }
    if (m_authority_node && m_authority_node->HasPendingOperation(op_id)) {
        return {.kind = OperationStatusKind::LOCAL_PENDING};
    }
    const auto known = m_recent_operation_status.find(op_id);
    if (known != m_recent_operation_status.end()) return known->second;
    return {};
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

void CybouNodeRuntime::RememberOperationStatus(const uint256& id, OperationStatus status)
{
    if (id.IsNull()) return;
    if (!m_recent_operation_status.contains(id)) m_recent_operation_status_order.push_back(id);
    m_recent_operation_status[id] = status;
    while (m_recent_operation_status_order.size() > 256) {
        m_recent_operation_status.erase(m_recent_operation_status_order.front());
        m_recent_operation_status_order.pop_front();
    }
}

std::vector<FinalizedHead> CybouNodeRuntime::RecentFinalizedBlocksForGossip() const
{
    std::lock_guard lock(m_mutex);
    return {m_recent_finalized_blocks.begin(), m_recent_finalized_blocks.end()};
}

void CybouNodeRuntime::RememberFinalizedBlockForGossip(const FinalizedBlock& block)
{
    const auto id = ComputeBlockId(block.block);
    if (id.IsNull()) return;
    if (!m_recent_finalized_blocks.empty() &&
        m_recent_finalized_blocks.back().height >= block.block.height) return;
    m_recent_finalized_blocks.push_back({id, block.block.height});
    if (m_recent_finalized_blocks.size() > 32) m_recent_finalized_blocks.pop_front();
}

OperationSubmitResult CybouNodeRuntime::SubmitOperationInternal(
    ProtocolOperation op, std::optional<std::string> source_peer)
{
    const uint256 op_id = ComputeOperationId(op).value_or(uint256{});
    std::optional<std::pair<std::string, uint16_t>> endpoint;
    std::optional<std::pair<std::string, uint16_t>> p2p_endpoint;
    uint256 net_id{};
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return OperationSubmitResult{.status = OperationSubmitStatus::NETWORK_MISMATCH, .op_id = op_id};
        }
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_store.PoaSafetyHalted() || (m_authority_node && m_authority_node->SafetyHalted())) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        if (m_authority_node) {
            const auto status = m_authority_node->SubmitOperationWithStatus(op, std::move(source_peer));
            if (status == OperationSubmitStatus::ACCEPTED || status == OperationSubmitStatus::ALREADY_PENDING) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::LOCAL_PENDING});
            } else if (status == OperationSubmitStatus::ALREADY_FINALIZED) {
                const auto height = m_store.GetFinalizedOperationHeight(op_id).value_or(0);
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED, .finalized_height = height});
            } else {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
            return OperationSubmitResult{.status = status, .op_id = op_id};
        }
        endpoint = m_submit_endpoint;
        p2p_endpoint = m_config.p2p_endpoint;
        net_id = m_network_id;
    }
    if (p2p_endpoint && m_peer_manager) {
        std::lock_guard p2p_lock(m_p2p_mutex);
        auto connected = m_peer_manager->Peers();
        if (connected.empty() && !m_peer_manager->Connect(p2p_endpoint->first, p2p_endpoint->second)) {
            return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
        }
        connected = m_peer_manager->Peers();
        std::vector<std::pair<std::string, uint16_t>> accepting_candidates;
        accepting_candidates.reserve(connected.size());
        for (const auto& peer : connected) accepting_candidates.emplace_back(peer.address, peer.port);
        const auto submitted = m_peer_manager->SubmitOperationToAny(accepting_candidates, op);
        auto result = submitted.acknowledgment.value_or(
            OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id});
        result.delivery_uncertain = submitted.delivery_uncertain;
        {
            std::lock_guard lock(m_mutex);
            if (result.status == OperationSubmitStatus::ACCEPTED ||
                result.status == OperationSubmitStatus::ALREADY_PENDING) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
            } else if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                    .finalized_height = m_store.GetFinalizedOperationHeight(op_id).value_or(0)});
            } else if (!result.delivery_uncertain) {
                RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
            }
        }
        return result;
    }
    if (endpoint.has_value()) {
        auto result = SubmitOperationRemote(endpoint->first, endpoint->second, net_id, op);
        std::lock_guard lock(m_mutex);
        if (result.status == OperationSubmitStatus::ACCEPTED ||
            result.status == OperationSubmitStatus::ALREADY_PENDING) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::ACCEPTED_REMOTE});
        } else if (result.status == OperationSubmitStatus::ALREADY_FINALIZED) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::FINALIZED,
                .finalized_height = m_store.GetFinalizedOperationHeight(op_id).value_or(0)});
        } else if (!result.delivery_uncertain) {
            RememberOperationStatus(op_id, {.kind = OperationStatusKind::REJECTED_KNOWN});
        }
        return result;
    }
    return OperationSubmitResult{.status = OperationSubmitStatus::REJECTED, .op_id = op_id};
}

std::optional<FinalizedBlock> CybouNodeRuntime::ProduceBlock(const bool sync)
{
    std::lock_guard lock(m_mutex);
    if (!m_authority_node) return std::nullopt;
    if (m_store.PoaSafetyHalted() || m_authority_node->SafetyHalted()) return std::nullopt;
    const auto loaded = m_store.LoadState();
    if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) return std::nullopt;
    const auto result = m_authority_node->ProduceNextBlock(sync);
    if (!result) return std::nullopt;
    if (result.finalized_block) RememberFinalizedBlockForGossip(*result.finalized_block);
    return result.finalized_block;
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
        RememberFinalizedBlockForGossip(block);
        if (m_authority_node) m_authority_node->RevalidatePending();
    }
    return result;
}

std::optional<FinalizedBlock> CybouNodeRuntime::GetBlockAtHeight(const uint64_t height) const
{
    std::lock_guard lock(m_mutex);
    return m_store.GetBlockAtHeight(height);
}

FinalizedOperationLookupResult CybouNodeRuntime::FindFinalizedOperation(const uint256& op_id) const
{
    FinalizedOperationLookupResult result;
    const auto status = GetStatus();
    if (!status.is_initialized || op_id.IsNull()) return result;
    result.status = FinalizedOperationLookupStatus::NOT_FOUND;
    uint256 previous_id = m_config.network_definition.genesis_block_id;
    for (uint64_t height = 1; height <= status.finalized_height; ++height) {
        const auto finalized = GetBlockAtHeight(height);
        if (!finalized) {
            result.status = FinalizedOperationLookupStatus::HISTORY_UNAVAILABLE;
            return result;
        }
        const auto block_id = ComputeBlockId(finalized->block);
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_id != status.network_id ||
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

std::optional<RootPublication> CybouNodeRuntime::FindFinalizedRootPublication(const uint256& op_id) const
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
    return publication ? std::optional<RootPublication>{publication->publication} : std::nullopt;
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
    uint256 previous_id = m_config.network_definition.genesis_block_id;
    for (uint64_t height = 1; height <= *finalized_height; ++height) {
        const auto finalized = m_store.GetBlockAtHeight(height);
        if (!finalized) return result;
        const auto block_id = ComputeBlockId(finalized->block);
        if (finalized->block.parent_block_id != previous_id ||
            finalized->certificate.network_id != m_network_id ||
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
            const auto account_bytes = account_id.Value();
            const auto commitment = ComputeIdentityKemPackageCommitment(
                std::span<const unsigned char, 32>{m_network_id.begin(), 32},
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

SyncPeerResult CybouNodeRuntime::SyncFromPeer(const std::string& host, const uint16_t port, const uint64_t max_blocks)
{
    SyncPeerResult result;
    {
        std::lock_guard lock(m_mutex);
        const auto loaded = m_store.LoadState();
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            result.status = SyncPeerStatus::NETWORK_MISMATCH;
            return result;
        }
        if (loaded.error != StateLoadError::NONE || !loaded.state.has_value()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            return result;
        }
    }
    while (result.blocks_applied < max_blocks) {
        uint64_t next_height{0};
        uint256 net_id{};
        {
            std::lock_guard lock(m_mutex);
            const auto height = m_store.GetFinalizedHeight();
            if (!height || *height == std::numeric_limits<uint64_t>::max()) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
                break;
            }
            next_height = *height + 1;
            net_id = m_network_id;
        }

        const auto fetch_res = FetchFinalizedBlock(host, port, net_id, next_height);
        if (fetch_res.status == FetchBlockStatus::NOT_FOUND) {
            if (result.blocks_applied == 0) {
                result.status = SyncPeerStatus::UP_TO_DATE;
            }
            break;
        }
        if (fetch_res.status == FetchBlockStatus::CONNECTION_FAILED) {
            result.status = SyncPeerStatus::CONNECTION_FAILED;
            break;
        }
        if (fetch_res.status == FetchBlockStatus::NETWORK_MISMATCH) {
            result.status = SyncPeerStatus::NETWORK_MISMATCH;
            break;
        }
        if (fetch_res.status == FetchBlockStatus::CORRUPT_BLOCK || !fetch_res.block.has_value()) {
            result.status = SyncPeerStatus::PROTOCOL_ERROR;
            break;
        }

        {
            std::lock_guard lock(m_mutex);
            if (!m_store.CommitFinalizedBlock(*fetch_res.block)) {
                result.status = SyncPeerStatus::PROTOCOL_ERROR;
                break;
            }
            RememberFinalizedBlockForGossip(*fetch_res.block);
            if (m_authority_node) m_authority_node->RevalidatePending();
        }
        ++result.blocks_applied;
        result.status = SyncPeerStatus::BLOCKS_APPLIED;
    }
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
    if (!m_config.p2p_endpoint || !m_peer_manager) return {};
    std::lock_guard p2p_lock(m_p2p_mutex);

    const auto local_status = GetStatus();
    if (local_status.runtime_state == NodeRuntimeState::NETWORK_MISMATCH) {
        return SyncPeerResult{.status = SyncPeerStatus::NETWORK_MISMATCH};
    }
    if (local_status.runtime_state == NodeRuntimeState::CORRUPT || !local_status.is_initialized) {
        return SyncPeerResult{.status = SyncPeerStatus::PROTOCOL_ERROR};
    }

    auto explicit_endpoints = GetExplicitPeerEndpoints();
    if (std::find(explicit_endpoints.begin(), explicit_endpoints.end(), *m_config.p2p_endpoint) ==
        explicit_endpoints.end()) {
        explicit_endpoints.push_back(*m_config.p2p_endpoint);
    }
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
    std::sort(peers.begin(), peers.end(), [](const p2p::PeerInfo& left, const p2p::PeerInfo& right) {
        if (left.hello.finalized_height != right.hello.finalized_height) {
            return left.hello.finalized_height > right.hello.finalized_height;
        }
        if (left.address != right.address) return left.address < right.address;
        return left.port < right.port;
    });

    SyncPeerResult result{.status = SyncPeerStatus::CONNECTION_FAILED};
    bool any_peer_up_to_date{false};
    for (const auto& peer : peers) {
        const auto attempt = m_peer_manager->SyncFromPeer(peer.address, peer.port, max_blocks);
        if (attempt.status == SyncPeerStatus::BLOCKS_APPLIED) {
            result = attempt;
            break;
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
    if (any_peer_up_to_date) result.status = SyncPeerStatus::UP_TO_DATE;
    m_peer_manager->FanoutRecentBlocks();
    return result;
}

size_t CybouNodeRuntime::ConnectedPeerCount() const
{
    std::lock_guard p2p_lock(m_p2p_mutex);
    return m_peer_manager ? m_peer_manager->ConnectedCount() : 0;
}

void CybouNodeRuntime::SetSubmitEndpoint(const std::string& host, const uint16_t port)
{
    std::lock_guard lock(m_mutex);
    m_submit_endpoint = std::make_pair(host, port);
}

bool CybouNodeRuntime::HasSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_authority_node != nullptr || m_submit_endpoint.has_value() || m_config.p2p_endpoint.has_value();
}

std::optional<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetSubmitEndpoint() const
{
    std::lock_guard lock(m_mutex);
    return m_submit_endpoint;
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetPeerEndpointsForGossip() const
{
    std::lock_guard lock(m_mutex);
    constexpr size_t MAX_GOSSIP_TARGETS{32};
    std::vector<std::pair<std::string, uint16_t>> result;
    const auto& configured = m_config.p2p_endpoint;
    if (configured.has_value()) {
        result.push_back(*configured);
    }
    // Explicit operator-configured peer endpoints come first: a flood of
    // malicious discovered hints must never eclipse the configured peer topology.
    for (const auto& ep : m_explicit_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if (!configured || ep != *configured) {
            result.push_back(ep);
        }
    }
    for (const auto& ep : m_discovered_peer_endpoints) {
        if (result.size() >= MAX_GOSSIP_TARGETS) break;
        if ((!configured || ep != *configured) &&
            std::find(result.begin(), result.end(), ep) == result.end()) {
            result.push_back(ep);
        }
    }
    return result;
}

void CybouNodeRuntime::SetExplicitPeerEndpoints(const std::vector<std::pair<std::string, uint16_t>>& endpoints)
{
    std::lock_guard lock(m_mutex);
    m_explicit_peer_endpoints.clear();
    for (const auto& [host, port] : endpoints) {
        if (port == 0) continue;
        boost::system::error_code ec;
        const auto addr = boost::asio::ip::make_address(host, ec);
        if (ec) continue;
        if (m_config.p2p_endpoint && addr.to_string() == m_config.p2p_endpoint->first &&
            port == m_config.p2p_endpoint->second) {
            continue;
        }
        if (m_config.local_p2p_endpoint && addr.to_string() == m_config.local_p2p_endpoint->first &&
            port == m_config.local_p2p_endpoint->second) {
            continue;
        }
        m_explicit_peer_endpoints.emplace(addr.to_string(), port);
    }
}

std::vector<std::pair<std::string, uint16_t>> CybouNodeRuntime::GetExplicitPeerEndpoints() const
{
    std::lock_guard lock(m_mutex);
    return {m_explicit_peer_endpoints.begin(), m_explicit_peer_endpoints.end()};
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
        if (!IsConnectableDiscoveredAddress(addr, m_config.local_p2p_endpoint)) continue;
        const auto canonical = std::make_pair(addr.to_string(), port);
        if (m_config.p2p_endpoint && canonical == *m_config.p2p_endpoint) continue;
        if (m_config.local_p2p_endpoint && canonical == *m_config.local_p2p_endpoint) continue;
        if (m_explicit_peer_endpoints.count(canonical) > 0) continue;
        if (m_discovered_peer_endpoints.size() >= MAX_DISCOVERED_PEER_ENDPOINTS) break;
        m_discovered_peer_endpoints.emplace(canonical);
    }
}

} // namespace cybou
