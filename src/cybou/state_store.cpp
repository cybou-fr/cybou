// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/signing.h>
#include <cybou/state_store.h>

#include <dbwrapper.h>

#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace cybou {
namespace {

const std::string STATE_KEY{"cybou/state/v1"};
const std::string HASH_KEY{"cybou/hash/v1"};
const std::string HEAD_KEY{"cybou/head/v1"};
const std::string NETWORK_ID_KEY{"cybou/network-id/v1"};

inline std::string BlockKey(const uint256& block_id)
{
    return "cybou/block/v1/" + block_id.GetHex();
}

inline std::string MailFilterKey(const uint256& block_id)
{
    return "cybou/mail-filter/v1/" + block_id.GetHex();
}

} // namespace

CybouStateStore::CybouStateStore(
    CDBWrapper& db,
    CybouNetworkDefinitionV1 network_definition,
    std::optional<OperatorAuthorityKeySet> operator_authority,
    std::shared_ptr<OperatorAuthoritySignatureVerifier> operator_verifier)
    : m_db{db},
      m_network_definition{std::move(network_definition)},
      m_network_definition_error{ValidateNetworkDefinition(m_network_definition)},
      m_network_id{NetworkId(m_network_definition)},
      m_operator_authority{std::move(operator_authority)},
      m_operator_verifier{std::move(operator_verifier)}
{
    if (!m_operator_verifier) {
        m_operator_verifier = std::make_shared<OpenSslOperatorAuthoritySignatureVerifier>();
    }
}

void CybouStateStore::SetOperatorAuthority(
    OperatorAuthorityKeySet keyset,
    std::shared_ptr<OperatorAuthoritySignatureVerifier> verifier)
{
    m_operator_authority = std::move(keyset);
    if (verifier) {
        m_operator_verifier = std::move(verifier);
    } else if (!m_operator_verifier) {
        m_operator_verifier = std::make_shared<OpenSslOperatorAuthoritySignatureVerifier>();
    }
}

std::optional<ValidatorSetV1> CybouStateStore::GetValidatorSet() const
{
    const auto loaded{LoadState()};
    if (!loaded) return std::nullopt;
    return loaded.state->validator_set;
}

GenesisInitResult CybouStateStore::InitializeGenesis(
    const CybouState& genesis_state,
    const bool sync)
{
    if (m_network_definition_error != NetworkDefinitionError::NONE) {
        return {GenesisInitError::INVALID_NETWORK_DEFINITION};
    }
    if (m_db.Exists(STATE_KEY) || m_db.Exists(HASH_KEY) || m_db.Exists(HEAD_KEY) ||
        m_db.Exists(NETWORK_ID_KEY)) {
        return {GenesisInitError::ALREADY_INITIALIZED};
    }
    if (CybouStateHash(genesis_state) != m_network_definition.genesis_state_root) {
        return {GenesisInitError::GENESIS_STATE_MISMATCH};
    }
    if (ComputeValidatorSetCommitment(genesis_state.validator_set) !=
        m_network_definition.initial_validator_set_commitment) {
        return {GenesisInitError::VALIDATOR_SET_COMMITMENT_MISMATCH};
    }
    if (ValidateValidatorSet(genesis_state.validator_set) != ValidatorSetValidationError::NONE) {
        return {GenesisInitError::INVALID_GENESIS_VALIDATOR_SET};
    }
    const FinalizedHeadV1 initial_head{
        .block_id = m_network_definition.genesis_block_id,
        .height = 0,
    };
    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(genesis_state));
    batch.Write(HASH_KEY, CybouStateHash(genesis_state));
    batch.Write(HEAD_KEY, initial_head);
    batch.Write(NETWORK_ID_KEY, m_network_id);
    const auto genesis_filter{BuildMailDiscoveryFilter(m_network_definition.genesis_block_id, {})};
    batch.Write(MailFilterKey(m_network_definition.genesis_block_id), SerializeMailDiscoveryFilter(genesis_filter));
    m_db.WriteBatch(batch, sync);
    return {};
}

StateLoadResult CybouStateStore::LoadState() const
{
    if (m_network_definition_error != NetworkDefinitionError::NONE) {
        return {StateLoadError::INVALID_NETWORK_DEFINITION, std::nullopt};
    }
    std::vector<unsigned char> bytes;
    uint256 stored_hash;
    const bool state_exists{m_db.Exists(STATE_KEY)};
    const bool hash_exists{m_db.Exists(HASH_KEY)};
    const bool head_exists{m_db.Exists(HEAD_KEY)};
    const bool network_exists{m_db.Exists(NETWORK_ID_KEY)};
    if (!state_exists && !hash_exists && !head_exists && !network_exists) {
        return {StateLoadError::NOT_FOUND, std::nullopt};
    }
    if (!state_exists || !hash_exists || !head_exists || !network_exists) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    const auto stored_network_id{GetStoredNetworkId()};
    if (!stored_network_id) return {StateLoadError::CORRUPT, std::nullopt};
    if (*stored_network_id != m_network_id) return {StateLoadError::NETWORK_MISMATCH, std::nullopt};

    const auto head{GetFinalizedHead()};
    if (!head) return {StateLoadError::CORRUPT, std::nullopt};

    const bool has_state{m_db.Read(STATE_KEY, bytes)};
    const bool has_hash{m_db.Read(HASH_KEY, stored_hash)};
    if (!has_state || !has_hash) return {StateLoadError::CORRUPT, std::nullopt};

    auto state{DeserializeCybouState(bytes)};
    if (!state || CybouStateHash(*state) != stored_hash) {
        return {StateLoadError::CORRUPT, std::nullopt};
    }
    return {StateLoadError::NONE, std::move(state)};
}

std::optional<uint256> CybouStateStore::GetStateRoot() const
{
    uint256 hash;
    if (!m_db.Read(HASH_KEY, hash)) return std::nullopt;
    return hash;
}

std::optional<FinalizedHeadV1> CybouStateStore::GetFinalizedHead() const
{
    FinalizedHeadV1 head;
    if (!m_db.Read(HEAD_KEY, head)) return std::nullopt;
    return head;
}

std::optional<uint256> CybouStateStore::GetFinalizedTip() const
{
    const auto head{GetFinalizedHead()};
    if (!head) return std::nullopt;
    return head->block_id;
}

std::optional<uint64_t> CybouStateStore::GetFinalizedHeight() const
{
    const auto head{GetFinalizedHead()};
    if (!head) return std::nullopt;
    return head->height;
}

std::optional<uint256> CybouStateStore::GetStoredNetworkId() const
{
    uint256 network_id;
    if (!m_db.Read(NETWORK_ID_KEY, network_id)) return std::nullopt;
    return network_id;
}

BlockTransitionResult CybouStateStore::CommitFinalizedBlock(
    const FinalizedBlockV1& finalized_block,
    const std::optional<ValidatorSetV1>& validator_set,
    const bool sync)
{
    const auto loaded{LoadState()};
    if (!loaded) {
        if (loaded.error == StateLoadError::INVALID_NETWORK_DEFINITION) {
            return {BlockTransitionError::INVALID_NETWORK_DEFINITION};
        }
        if (loaded.error == StateLoadError::NETWORK_MISMATCH) {
            return {BlockTransitionError::NETWORK_MISMATCH};
        }
        if (loaded.error == StateLoadError::CORRUPT) {
            return {BlockTransitionError::CORRUPT_STATE};
        }
        return {BlockTransitionError::STATE_NOT_INITIALIZED};
    }

    if (validator_set.has_value() && *validator_set != loaded.state->validator_set) {
        return {BlockTransitionError::VALIDATOR_SET_MISMATCH};
    }

    const auto& block = finalized_block.block;
    const auto& cert = finalized_block.certificate;
    const uint256 block_id = ComputeBlockId(block);
    if (block_id.IsNull()) {
        return {BlockTransitionError::INVALID_BLOCK_ID};
    }

    const auto head{GetFinalizedHead()};
    if (!head) return {BlockTransitionError::CORRUPT_HEAD};
    if (head->block_id == block_id) return {BlockTransitionError::BLOCK_ALREADY_APPLIED};
    if (head->block_id != block.parent_block_id) {
        return {BlockTransitionError::PARENT_MISMATCH};
    }
    if (head->height == std::numeric_limits<uint64_t>::max() || block.height != head->height + 1) {
        return {BlockTransitionError::INVALID_HEIGHT};
    }

    if (cert.block_id != block_id || cert.height != block.height) {
        return {.error = BlockTransitionError::INVALID_CERTIFICATE};
    }

    const auto cert_res = VerifyFinalityCertificate(cert, loaded.state->validator_set, m_network_id);
    if (cert_res != FinalityVerificationError::NONE) {
        return {.error = BlockTransitionError::INVALID_CERTIFICATE, .cert_error = cert_res};
    }

    const auto& params{m_network_definition.protocol_parameters};
    const size_t account_create_count{static_cast<size_t>(std::count_if(
        block.operations.begin(), block.operations.end(), [](const auto& operation) {
            return OperationType(operation) == ProtocolOperationType::ACCOUNT_CREATE;
        }))};
    if (account_create_count > params.max_account_creates_per_block) {
        return {BlockTransitionError::TOO_MANY_ACCOUNT_CREATES};
    }

    auto candidate{*loaded.state};
    const ProtocolExecutionContextV1 ctx{
        .network_id = m_network_id,
        .block_height = block.height,
        .params = params,
        .operator_authority = m_operator_authority ? &*m_operator_authority : nullptr,
        .operator_verifier = m_operator_verifier.get(),
    };
    for (const auto& operation : block.operations) {
        const auto res{ApplyProtocolOperation(operation, ctx, candidate)};
        if (!res) return {BlockTransitionError::INVALID_OPERATION, res};
    }

    if (candidate.pending_fee_pool > 0) {
        const auto fee_res = RoutePendingFees(candidate);
        if (!fee_res) {
            return {BlockTransitionError::FEE_ROUTING_FAILED};
        }
    }

    const uint256 candidate_root = CybouStateHash(candidate);
    if (candidate_root != block.resulting_state_root) {
        return {BlockTransitionError::STATE_ROOT_MISMATCH};
    }

    const FinalizedHeadV1 next_head{
        .block_id = block_id,
        .height = block.height,
    };

    CDBBatch batch{m_db};
    batch.Write(STATE_KEY, SerializeCybouState(candidate));
    batch.Write(HASH_KEY, candidate_root);
    batch.Write(HEAD_KEY, next_head);
    batch.Write(BlockKey(block_id), SerializeFinalizedBlock(finalized_block));
    const auto mail_filter{BuildBlockMailDiscoveryFilter(block)};
    batch.Write(MailFilterKey(block_id), SerializeMailDiscoveryFilter(mail_filter));
    m_db.WriteBatch(batch, sync);
    return {};
}

std::optional<FinalizedBlockV1> CybouStateStore::GetBlock(const uint256& block_id) const
{
    std::vector<unsigned char> bytes;
    if (!m_db.Read(BlockKey(block_id), bytes)) {
        return std::nullopt;
    }
    return DeserializeFinalizedBlock(bytes);
}

std::optional<CybouMailDiscoveryFilterV1> CybouStateStore::GetBlockMailFilter(const uint256& block_id) const
{
    std::vector<unsigned char> bytes;
    if (!m_db.Read(MailFilterKey(block_id), bytes)) {
        return std::nullopt;
    }
    auto filter{DeserializeMailDiscoveryFilter(bytes)};
    if (filter && filter->block_id != block_id) return std::nullopt;
    return filter;
}

} // namespace cybou
