// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief StorageLease и StorageSettlement: аренда хранения через escrow и PoA-выплаты providers.

#include <cybou/storage_lease.h>

#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>
#include <cybou/storage_economy.h>
#include <cybou/storage_assignment.h>
#include <cybou/chunk_authorization.h>
#include <cybou/binary_codec.h>
#include <set>
#include <cybou/protocol_operation.h>
#include <stdexcept>

#include <algorithm>
#include <limits>
#include <map>
#include <string_view>
#include <tuple>

namespace cybou {
namespace {

constexpr size_t SETTLEMENT_HEADER_SIZE{8 + 8 + 1 + 8 + 32 + 4};
constexpr size_t POA_SIGNATURE_SIZE{64 + 3309};

void Write32(std::vector<unsigned char>& out, uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

void Write64(std::vector<unsigned char>& out, uint64_t value)
{
    for (unsigned i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint32_t Read32(std::span<const unsigned char> bytes)
{
    uint32_t value{0};
    for (unsigned i{0}; i < 4; ++i) value |= uint32_t{bytes[i]} << (8 * i);
    return value;
}

uint64_t Read64(std::span<const unsigned char> bytes)
{
    uint64_t value{0};
    for (unsigned i{0}; i < 8; ++i) value |= uint64_t{bytes[i]} << (8 * i);
    return value;
}

bool CheckedMul(uint64_t a, uint64_t b, uint64_t& out)
{
    if (a != 0 && b > std::numeric_limits<uint64_t>::max() / a) return false;
    out = a * b;
    return true;
}

std::optional<std::vector<unsigned char>> SerializeSettlementBody(const StorageSettlement& settlement)
{
    if (settlement.period_start_utc == 0 || settlement.entries.size() > MAX_STORAGE_SETTLEMENT_ENTRIES) {
        return std::nullopt;
    }
    if (settlement.action != StorageSettlementAction::PAY) {
        if (!settlement.entries.empty() || settlement.period_end_utc || !settlement.evidence_root.IsNull() ||
            !settlement.activation_witnesses.empty()) return std::nullopt;
        try {
            BinaryWriter out{MAX_OPERATION_PAYLOAD_BYTES - 1 - POA_SIGNATURE_SIZE};
            out.U64(settlement.period); out.U64(settlement.period_start_utc); out.U8(static_cast<uint8_t>(settlement.action));
            if (settlement.action == StorageSettlementAction::PREPARE) {
                if (settlement.funded_term_id.IsNull() || !settlement.preparation_id.IsNull() || !settlement.assignment_epoch ||
                    !settlement.manifest.empty() || settlement.eligible.empty() || settlement.eligible.size() > 20) return std::nullopt;
                out.Fixed({settlement.funded_term_id.begin(), 32}); out.U64(settlement.assignment_epoch);
                out.U32(static_cast<uint32_t>(settlement.eligible.size()));
                const StorageSettlementBinding* prior{nullptr};
                for (const auto& e : settlement.eligible) {
                    if (e.storage_id == std::array<unsigned char,32>{} || (prior && !(prior->storage_id < e.storage_id))) return std::nullopt;
                    prior = &e;
                    const auto binding = EncodeStoragePayoutBinding(e.binding);
                    if (binding.size() != 6344 || !DecodeStoragePayoutBinding(binding)) return std::nullopt;
                    out.Fixed(e.storage_id); out.Bytes(binding, 6344);
                }
            } else if (settlement.action == StorageSettlementAction::ACTIVATE) {
                if (settlement.preparation_id.IsNull() || !settlement.funded_term_id.IsNull() || settlement.assignment_epoch ||
                    !settlement.eligible.empty() || settlement.manifest.empty() || settlement.manifest.size() > 3988) return std::nullopt;
                out.Fixed({settlement.preparation_id.begin(), 32}); out.U32(static_cast<uint32_t>(settlement.manifest.size()));
                std::set<ChunkId> seen;
                for (const auto& chunk : settlement.manifest) {
                    if (chunk == ChunkId{} || !seen.insert(chunk).second) return std::nullopt;
                    out.Fixed(chunk);
                }
            } else return std::nullopt;
            return out.Take();
        } catch (const std::invalid_argument&) { return std::nullopt;
        } catch (const std::length_error&) { return std::nullopt; }
    }
    if (!settlement.funded_term_id.IsNull() || !settlement.preparation_id.IsNull() || settlement.assignment_epoch ||
        !settlement.eligible.empty() || !settlement.manifest.empty()) return std::nullopt;
    std::vector<unsigned char> out;
    if (settlement.period_end_utc <= settlement.period_start_utc || settlement.evidence_root.IsNull() ||
        settlement.activation_witnesses.size() > (MAX_OPERATION_PAYLOAD_BYTES / 32)) return std::nullopt;
    const size_t size = SETTLEMENT_HEADER_SIZE + settlement.entries.size() * STORAGE_SETTLEMENT_ENTRY_SIZE +
        4 + 32 * settlement.activation_witnesses.size();
    if (size > MAX_OPERATION_PAYLOAD_BYTES - 1 - POA_SIGNATURE_SIZE) return std::nullopt;
    out.reserve(size);
    Write64(out, settlement.period);
    Write64(out, settlement.period_start_utc);
    out.push_back(static_cast<uint8_t>(settlement.action));
    Write64(out, settlement.period_end_utc);
    out.insert(out.end(), settlement.evidence_root.begin(), settlement.evidence_root.end());
    Write32(out, static_cast<uint32_t>(settlement.entries.size()));
    const StorageSettlementEntry* prior{nullptr};
    for (const auto& entry : settlement.entries) {
        if (entry.funding_operation_id.IsNull() || entry.payout_account.IsNull() ||
            entry.storage_id == std::array<unsigned char,32>{} || !entry.verified_unit_seconds ||
            (prior && !(std::tie(prior->funding_operation_id, prior->slot, prior->storage_id, prior->payout_account) <
                        std::tie(entry.funding_operation_id, entry.slot, entry.storage_id, entry.payout_account)))) return std::nullopt;
        prior = &entry;
        out.insert(out.end(), entry.funding_operation_id.begin(), entry.funding_operation_id.end());
        out.push_back(entry.slot);
        out.insert(out.end(), entry.storage_id.begin(), entry.storage_id.end());
        out.insert(out.end(), entry.payout_account.Value().begin(), entry.payout_account.Value().end());
        Write64(out, entry.verified_unit_seconds);
        Write64(out, entry.amount);
    }
    Write32(out, static_cast<uint32_t>(settlement.activation_witnesses.size()));
    const Hash256* previous{nullptr};
    for (const auto& id : settlement.activation_witnesses) {
        if (id.IsNull() || (previous && !(*previous < id))) return std::nullopt;
        out.insert(out.end(), id.begin(), id.end()); previous = &id;
    }
    return out;
}

} // namespace

std::optional<std::array<unsigned char, STORAGE_LEASE_PAYLOAD_SIZE>> SerializeStorageLeasePayload(
    const StorageLeasePayload& lease)
{
    if (lease.publication_id.IsNull() || lease.periods == 0) return std::nullopt;
    std::array<unsigned char, STORAGE_LEASE_PAYLOAD_SIZE> out{};
    std::copy(lease.publication_id.begin(), lease.publication_id.end(), out.begin());
    for (unsigned i{0}; i < 4; ++i) out[32 + i] = static_cast<unsigned char>(lease.periods >> (8 * i));
    return out;
}

std::optional<StorageLeasePayload> DeserializeStorageLeasePayload(std::span<const unsigned char> bytes)
{
    if (bytes.size() != STORAGE_LEASE_PAYLOAD_SIZE) return std::nullopt;
    StorageLeasePayload lease;
    std::copy_n(bytes.begin(), 32, lease.publication_id.begin());
    lease.periods = Read32(bytes.subspan(32, 4));
    if (!SerializeStorageLeasePayload(lease)) return std::nullopt;
    return lease;
}

std::optional<IdentityKeyId> ComputeStorageLeasePayloadCommitment(const StorageLeasePayload& lease)
{
    constexpr std::string_view domain{"CYBOU/STORAGE-LEASE-PAYLOAD"};
    const auto encoded = SerializeStorageLeasePayload(lease);
    if (!encoded) return std::nullopt;
    IdentityKeyId digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), std::span<const unsigned char>{*encoded}}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

std::optional<uint64_t> ComputeStorageLeaseEscrow(const CybouProtocolParameters& params, uint32_t units,
    uint8_t replicas, uint32_t periods)
{
    const auto budget = ComputeAssignedStorageBudget(units, replicas, periods,
        params.storage_settlement_period_seconds, params.storage_rate_per_gib_day_replica);
    return budget ? std::optional<uint64_t>{budget->total} : std::nullopt;
}

std::optional<uint64_t> ComputeAcceptedStorageCapacity(const StorageFundedTerm& term,
    const uint8_t slot, const std::array<unsigned char, 32>& storage_id,
    const AccountId& payout_account, const uint64_t through_period, const uint64_t closure_period)
{
    if (!term.period_seconds || term.end_period <= term.first_period ||
        storage_id == std::array<unsigned char, 32>{} || payout_account.IsNull() ||
        through_period < term.first_period || closure_period < term.first_period) return std::nullopt;
    const auto end = std::min({through_period, closure_period, term.end_period});
    uint64_t capacity{0};
    for (size_t i = 0; i < term.assignments.size(); ++i) {
        const auto& epoch = term.assignments[i];
        if (!epoch.epoch || epoch.effective_period < term.first_period || epoch.effective_period >= term.end_period ||
            (i && (epoch.epoch <= term.assignments[i - 1].epoch ||
                   epoch.effective_period < term.assignments[i - 1].effective_period))) return std::nullopt;
        uint32_t units{0};
        bool found{false};
        for (const auto& allocation : epoch.allocations) {
            if (allocation.slot != slot || allocation.storage_id != storage_id || allocation.payout_account != payout_account) continue;
            if (found || !allocation.units) return std::nullopt;
            units = allocation.units;
            found = true;
        }
        const auto epoch_end = i + 1 < term.assignments.size()
            ? std::min(end, term.assignments[i + 1].effective_period) : end;
        if (epoch_end <= epoch.effective_period || !units) continue;
        uint64_t seconds{0}, increment{0};
        if (!CheckedMul(epoch_end - epoch.effective_period, term.period_seconds, seconds) ||
            !CheckedMul(seconds, units, increment) || increment > std::numeric_limits<uint64_t>::max() - capacity)
            return std::nullopt;
        capacity += increment;
    }
    if (capacity > term.contracted_unit_seconds) return std::nullopt;
    return capacity;
}

void FundStorageLease(CybouState& state, const cybou::Hash256& publication_id, const AccountId& payer,
    const uint32_t units, const uint8_t replicas, const uint32_t periods, const uint64_t escrow,
    const CybouProtocolParameters& params, const cybou::Hash256& funding_operation_id)
{
    const auto budget = ComputeAssignedStorageBudget(units, replicas, periods,
        params.storage_settlement_period_seconds, params.storage_rate_per_gib_day_replica);
    const auto existing = state.leases.find(publication_id);
    const auto first = existing == state.leases.end() ? state.settlement.next_period : std::max(state.settlement.next_period, existing->second.end_period);
    if (!budget || budget->total != escrow || funding_operation_id.IsNull() ||
        first > std::numeric_limits<uint64_t>::max() - periods ||
        (existing != state.leases.end() &&
            (existing->second.funded_terms.empty() || existing->second.payer != payer || existing->second.units != units || existing->second.replicas != replicas ||
             std::any_of(existing->second.funded_terms.begin(), existing->second.funded_terms.end(),
                [&](const auto& term) { return term.funding_operation_id == funding_operation_id; })))) {
        throw std::invalid_argument{"invalid or duplicate funded storage term"};
    }
    // Onboarding-происхождение уходит в escrow первым и сохраняется для выплат (DEC-281).
    const uint64_t onboarding = DebitSystemBalance(state.accounts.at(payer), escrow);
    StorageFundedTerm term{.funding_operation_id = funding_operation_id, .first_period = first,
        .end_period = first + periods, .rate = params.storage_rate_per_gib_day_replica,
        .period_seconds = params.storage_settlement_period_seconds, .replica_share = budget->per_replica,
        .contracted_unit_seconds = budget->contracted_unit_seconds,
        .initial_onboarding = onboarding, .initial_locked = escrow - onboarding};
    if (existing != state.leases.end()) {
        existing->second.end_period = first + periods;
        existing->second.escrow_onboarding += onboarding;
        existing->second.escrow_locked += escrow - onboarding;
        existing->second.funded_terms.push_back(term);
        return;
    }
    state.leases.emplace(publication_id, StorageLeaseRecord{.payer = payer, .units = units, .replicas = replicas,
        .first_period = first, .end_period = term.end_period, .escrow_onboarding = onboarding,
        .escrow_locked = escrow - onboarding, .funded_terms = {term}});
}

StorageLeaseError ApplyStorageLease(const AuthorizedStorageLease& op, const cybou::Hash256& network_binding,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.authorization.kind != IdentityOperationKind::STORAGE_LEASE) return StorageLeaseError::INVALID_AUTHORIZATION;
    const auto commitment = ComputeStorageLeasePayloadCommitment(op.lease);
    if (!commitment || op.authorization.payload_commitment != *commitment ||
        op.lease.periods > params.max_storage_lease_periods) return StorageLeaseError::INVALID_PAYLOAD;
    const auto& account_id = op.authorization.account_id;
    auto sender = state.accounts.find(account_id);
    if (sender == state.accounts.end() || !state.identities.Find(account_id)) return StorageLeaseError::SENDER_NOT_FOUND;
    const auto publication = state.publications.find(op.lease.publication_id);
    if (publication == state.publications.end()) return StorageLeaseError::PUBLICATION_NOT_FOUND;
    if (publication->second.owner != account_id) return StorageLeaseError::NOT_OWNER;

    const auto existing = state.leases.find(op.lease.publication_id);
    const uint8_t replicas = existing != state.leases.end() ? existing->second.replicas : params.storage_replica_target;
    const auto escrow = ComputeStorageLeaseEscrow(params, publication->second.chunk_count, replicas, op.lease.periods);
    if (!escrow || *escrow > std::numeric_limits<uint64_t>::max() - params.payment_fee) {
        return StorageLeaseError::ESCROW_OVERFLOW;
    }
    const auto first = existing == state.leases.end() ? state.settlement.next_period : std::max(state.settlement.next_period, existing->second.end_period);
    const auto funding_id = ComputeOperationId(ProtocolOperation{op});
    if (!funding_id || (existing != state.leases.end() && existing->second.funded_terms.empty()))
        return StorageLeaseError::INVALID_PAYLOAD;
    if (first > std::numeric_limits<uint64_t>::max() - op.lease.periods)
        return StorageLeaseError::ESCROW_OVERFLOW;
    if (existing != state.leases.end() && (existing->second.end_period >
            std::numeric_limits<uint64_t>::max() - op.lease.periods ||
        existing->second.escrow_onboarding + existing->second.escrow_locked >
            std::numeric_limits<uint64_t>::max() - *escrow)) return StorageLeaseError::ESCROW_OVERFLOW;
    if (sender->second.system_balance < *escrow + params.payment_fee) return StorageLeaseError::INSUFFICIENT_SYSTEM_BALANCE;
    if (!CanCreditCentralAuthorityFee(state, params.payment_fee)) return StorageLeaseError::FEE_TRANSFER_FAILED;
    if (state.identities.AuthorizeOperation(op.authorization, network_binding) != IdentityRegistryError::NONE) {
        return StorageLeaseError::INVALID_AUTHORIZATION;
    }

    // Обычная комиссия идёт в Treasury; rent уходит в escrow, а не в Treasury (DEC-278).
    DebitSystemBalance(sender->second, params.payment_fee);
    CreditCentralAuthorityFee(state, params.payment_fee);
    FundStorageLease(state, op.lease.publication_id, account_id, publication->second.chunk_count, replicas,
        op.lease.periods, *escrow, params, *funding_id);
    return StorageLeaseError::NONE;
}

std::optional<std::array<unsigned char, 32>> ComputeStorageSettlementDigest(
    const cybou::Hash256& network_binding, const StorageSettlement& settlement)
{
    constexpr std::string_view domain{"CYBOU/STORAGE-SETTLEMENT"};
    const auto body = SerializeSettlementBody(settlement);
    if (network_binding.IsNull() || !body) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain),
            std::span<const unsigned char>{network_binding.begin(), 32}, std::span<const unsigned char>{*body}},
            digest.data())) return std::nullopt;
    return digest;
}

std::optional<std::vector<unsigned char>> SerializeStorageSettlement(const StorageSettlement& settlement)
{
    auto out = SerializeSettlementBody(settlement);
    if (!out || settlement.poa_signature.ml_dsa.size() != 3309) return std::nullopt;
    out->insert(out->end(), settlement.poa_signature.ed25519.begin(), settlement.poa_signature.ed25519.end());
    out->insert(out->end(), settlement.poa_signature.ml_dsa.begin(), settlement.poa_signature.ml_dsa.end());
    return out;
}

std::optional<StorageSettlement> DeserializeStorageSettlement(std::span<const unsigned char> bytes)
{
    if (bytes.size() < 17 + POA_SIGNATURE_SIZE || bytes.size() > MAX_OPERATION_PAYLOAD_BYTES - 1) return std::nullopt;
    StorageSettlement settlement{.period = Read64(bytes.subspan(0, 8)), .period_start_utc = Read64(bytes.subspan(8, 8))};
    settlement.action = static_cast<StorageSettlementAction>(bytes[16]);
    if (settlement.action != StorageSettlementAction::PAY) {
        try {
            BinaryReader in{bytes.subspan(17, bytes.size() - 17 - POA_SIGNATURE_SIZE)};
            if (bytes.size() < 17 + 36 + POA_SIGNATURE_SIZE) return std::nullopt;
            if (settlement.action == StorageSettlementAction::PREPARE) {
                if (bytes.size() < 17 + 44 + POA_SIGNATURE_SIZE) return std::nullopt;
                const auto id = in.Fixed(32); std::copy(id.begin(), id.end(), settlement.funded_term_id.begin());
                settlement.assignment_epoch = in.U64(); const auto count = in.U32();
                if (!count || count > 20 || count > (bytes.size() - POA_SIGNATURE_SIZE - 17 - 44) / 6380) return std::nullopt;
                for (uint32_t i{0}; i < count; ++i) {
                    StorageSettlementBinding e; e.storage_id = in.Fixed<std::array<unsigned char,32>>();
                    const auto binding = DecodeStoragePayoutBinding(in.Bytes(6344)); if (!binding) return std::nullopt;
                    e.binding = *binding; settlement.eligible.push_back(std::move(e));
                }
            } else if (settlement.action == StorageSettlementAction::ACTIVATE) {
                const auto id = in.Fixed(32); std::copy(id.begin(), id.end(), settlement.preparation_id.begin());
                const auto count = in.U32(); if (!count || count > 3988 || count > (bytes.size() - POA_SIGNATURE_SIZE - 17 - 36) / 32) return std::nullopt;
                for (uint32_t i{0}; i < count; ++i) settlement.manifest.push_back(in.Fixed<ChunkId>());
            } else return std::nullopt;
            in.Finish();
            const auto signature = bytes.last(POA_SIGNATURE_SIZE);
            std::copy_n(signature.begin(), 64, settlement.poa_signature.ed25519.begin());
            settlement.poa_signature.ml_dsa.assign(signature.begin() + 64, signature.end());
            if (!SerializeSettlementBody(settlement)) return std::nullopt;
            return settlement;
        } catch (const std::invalid_argument&) { return std::nullopt;
        } catch (const std::length_error&) { return std::nullopt; }
    }
    if (bytes.size() < SETTLEMENT_HEADER_SIZE + 4 + POA_SIGNATURE_SIZE) return std::nullopt;
    settlement.period_end_utc = Read64(bytes.subspan(17, 8));
    std::copy_n(bytes.begin() + 25, 32, settlement.evidence_root.begin());
    const uint32_t count = Read32(bytes.subspan(57, 4));
    if (count > MAX_STORAGE_SETTLEMENT_ENTRIES ||
        count > (bytes.size() - SETTLEMENT_HEADER_SIZE - 4 - POA_SIGNATURE_SIZE) / STORAGE_SETTLEMENT_ENTRY_SIZE)
        return std::nullopt;
    size_t offset{SETTLEMENT_HEADER_SIZE};
    for (uint32_t i{0}; i < count; ++i) {
        StorageSettlementEntry entry;
        std::copy_n(bytes.begin() + offset, 32, entry.funding_operation_id.begin());
        entry.slot = bytes[offset + 32];
        std::copy_n(bytes.begin() + offset + 33, 32, entry.storage_id.begin());
        const auto payout = AccountId::FromBytes(bytes.subspan(offset + 65, 32));
        if (!payout) return std::nullopt;
        entry.payout_account = *payout;
        entry.verified_unit_seconds = Read64(bytes.subspan(offset + 97, 8));
        entry.amount = Read64(bytes.subspan(offset + 105, 8));
        settlement.entries.push_back(entry);
        offset += STORAGE_SETTLEMENT_ENTRY_SIZE;
    }
    const uint32_t witnesses = Read32(bytes.subspan(offset, 4)); offset += 4;
    if (witnesses > (bytes.size() - offset - POA_SIGNATURE_SIZE) / 32 ||
        bytes.size() != offset + size_t{witnesses} * 32 + POA_SIGNATURE_SIZE) return std::nullopt;
    for (uint32_t i{0}; i < witnesses; ++i) {
        Hash256 id; std::copy_n(bytes.begin() + offset, 32, id.begin()); offset += 32;
        settlement.activation_witnesses.push_back(id);
    }
    std::copy_n(bytes.begin() + offset, 64, settlement.poa_signature.ed25519.begin());
    settlement.poa_signature.ml_dsa.assign(bytes.begin() + offset + 64, bytes.end());
    if (!SerializeSettlementBody(settlement)) return std::nullopt;
    return settlement;
}

namespace {
StorageSettlementError ApplyAssignmentInputs(const StorageSettlement& op, const Hash256& network,
    CybouState& state, uint64_t height, const Hash256& parent, bool apply)
{
    if (!SerializeSettlementBody(op) || network.IsNull() || !height) return StorageSettlementError::INVALID_PAYLOAD;
    if (op.period != state.settlement.next_period) return StorageSettlementError::WRONG_PERIOD;
    if (state.settlement.next_period_start_utc && state.settlement.next_period_start_utc != op.period_start_utc)
        return StorageSettlementError::WRONG_PERIOD_START;
    StorageLeaseRecord* lease{nullptr}; StorageFundedTerm* term{nullptr}; Hash256 publication;
    const StorageAssignmentDeclaration* declaration{nullptr};
    for (auto& [id, l] : state.leases) for (auto& t : l.funded_terms) {
        if (op.action == StorageSettlementAction::PREPARE && t.funding_operation_id == op.funded_term_id) {
            if (term) return StorageSettlementError::INVALID_ASSIGNMENT;
            lease = &l; term = &t; publication = id;
        } else if (op.action == StorageSettlementAction::ACTIVATE) {
            for (const auto& d : t.declarations) if (d.operation_id == op.preparation_id) {
                if (term) return StorageSettlementError::INVALID_ASSIGNMENT;
                lease = &l; term = &t; publication = id; declaration = &d;
            }
        }
    }
    if (!term) return StorageSettlementError::LEASE_NOT_FOUND;
    const auto pub = state.publications.find(publication);
    if (pub == state.publications.end() || op.period < term->first_period || op.period >= term->end_period ||
        op.period >= lease->end_period) return StorageSettlementError::LEASE_NOT_ACTIVE;
    if (op.period_start_utc > std::numeric_limits<uint64_t>::max() - term->period_seconds)
        return StorageSettlementError::INVALID_PAYLOAD;
    const auto operation_id = apply ? ComputeOperationId(ProtocolOperation{op}) : std::optional<Hash256>{};
    if (apply && !operation_id) return StorageSettlementError::INVALID_PAYLOAD;
    if (op.action == StorageSettlementAction::PREPARE) {
        if (lease->replicas > 2 || op.assignment_epoch < term->next_assignment_epoch || op.assignment_epoch == std::numeric_limits<uint64_t>::max())
            return StorageSettlementError::INVALID_ASSIGNMENT;
        StorageAssignmentDeclaration accepted{.epoch = op.assignment_epoch, .height = height};
        std::set<AccountId> accounts;
        for (const auto& e : op.eligible) {
            const auto* identity = state.identities.Find(e.binding.payout_account);
            if (!identity || !state.accounts.contains(e.binding.payout_account)) return StorageSettlementError::PAYOUT_ACCOUNT_NOT_FOUND;
            if (e.binding.payout_account == lease->payer) return StorageSettlementError::SELF_PAYOUT;
            if (!VerifyStoragePayoutBindingStorageKey(e.binding, network, e.storage_id) ||
                identity->authorization_key.purpose != IdentityKeyPurpose::AUTHORIZATION ||
                !VerifyIdentityMessage(identity->authorization_key, e.binding.authorization,
                    StoragePayoutBindingDigest(network, e.storage_id, e.binding.payout_account))) return StorageSettlementError::INVALID_ASSIGNMENT;
            accepted.eligible.push_back({e.storage_id, e.binding.payout_account, identity->key_epoch, height});
            accounts.insert(e.binding.payout_account);
        }
        if (accounts.size() < lease->replicas) return StorageSettlementError::INVALID_ASSIGNMENT;
        if (apply) {
            accepted.operation_id = *operation_id;
            term->declarations.push_back(std::move(accepted)); term->next_assignment_epoch = op.assignment_epoch + 1;
        }
    } else {
        if (!declaration || term->declarations.back().operation_id != op.preparation_id || parent.IsNull() ||
            declaration->height > std::numeric_limits<uint64_t>::max() - 2 || height != declaration->height + 2)
            return StorageSettlementError::WRONG_ACTIVATION_HEIGHT;
        if (!term->assignments.empty() && declaration->epoch <= term->assignments.back().epoch) return StorageSettlementError::INVALID_ASSIGNMENT;
        if (op.manifest.size() != lease->units || op.manifest.size() != pub->second.chunk_count) return StorageSettlementError::INVALID_ASSIGNMENT;
        std::vector<AuthorizedChunk> leaves;
        for (const auto& id : op.manifest) leaves.push_back({id});
        const auto tree = BuildChunkAuthorizationTree(leaves);
        if (!tree || tree->root != pub->second.chunk_authorization_root) return StorageSettlementError::INVALID_ASSIGNMENT;
        StorageAssignmentContext context;
        std::copy_n(network.begin(), 32, context.network_binding.begin());
        std::copy_n(publication.begin(), 32, context.publication.begin());
        std::copy_n(parent.begin(), 32, context.finalized_seed.begin());
        std::copy_n(lease->payer.Value().begin(), 32, context.payer.begin()); context.epoch = declaration->epoch;
        context.term_start = term->first_period; context.term_end = term->end_period; context.replicas = lease->replicas;
        std::vector<StorageAssignmentProvider> eligible;
        for (const auto& e : declaration->eligible) {
            StorageAssignmentProvider provider{.storage_id = e.storage_id};
            std::copy_n(e.payout_account.Value().begin(), 32, provider.payout_account.begin());
            eligible.push_back(provider);
        }
        std::map<std::tuple<uint8_t,std::array<unsigned char,32>,AccountId>,uint32_t> counts;
        for (const auto& chunk : op.manifest) {
            context.chunk = chunk;
            const auto plan = PrepareStorageAssignment(context, eligible);
            if (!plan) return StorageSettlementError::INVALID_ASSIGNMENT;
            for (uint8_t slot{0}; slot < plan->selected.size(); ++slot) {
                const auto& e = plan->selected[slot]; const auto account = AccountId::FromBytes(e.payout_account);
                if (!account) return StorageSettlementError::INVALID_ASSIGNMENT;
                ++counts[{slot,e.storage_id,*account}];
            }
        }
        StorageAcceptedAssignment accepted{.preparation_id = op.preparation_id, .seed = parent,
            .epoch = declaration->epoch, .effective_period = op.period};
        for (const auto& [key, units] : counts) accepted.allocations.push_back({std::get<0>(key), std::get<1>(key), std::get<2>(key), units});
        if (apply) {
            accepted.operation_id = *operation_id;
            for (auto& e : term->service_payments) {
                const auto capacity = ComputeAcceptedStorageCapacity(*term, e.slot, e.storage_id, e.payout_account, op.period, lease->end_period);
                if (!capacity) return StorageSettlementError::INVALID_ASSIGNMENT;
                e.closed_epoch_capacity = *capacity;
            }
            term->assignments.push_back(std::move(accepted));
        }
    }
    if (apply && !state.settlement.next_period_start_utc) state.settlement.next_period_start_utc = op.period_start_utc;
    return StorageSettlementError::NONE;
}

StorageSettlementError ApplyPayInputs(const StorageSettlement& op,
    const CybouProtocolParameters& params, CybouState& state)
{
    if (op.action != StorageSettlementAction::PAY || !SerializeSettlementBody(op)) return StorageSettlementError::INVALID_PAYLOAD;
    if (op.period != state.settlement.next_period) return StorageSettlementError::WRONG_PERIOD;
    if (state.settlement.next_period_start_utc && op.period_start_utc != state.settlement.next_period_start_utc)
        return StorageSettlementError::WRONG_PERIOD_START;
    if (!params.storage_settlement_period_seconds || op.period == std::numeric_limits<uint64_t>::max() ||
        op.period_start_utc > std::numeric_limits<uint64_t>::max() - params.storage_settlement_period_seconds ||
        op.period_end_utc != op.period_start_utc + params.storage_settlement_period_seconds)
        return StorageSettlementError::INVALID_PAYLOAD;
    std::set<Hash256> used_witnesses;
    const auto key = [](const auto& e) { return std::tie(e.slot, e.storage_id, e.payout_account); };
    for (const auto& entry : op.entries) {
        StorageLeaseRecord* lease{nullptr}; StorageFundedTerm* term{nullptr};
        for (auto& [id, l] : state.leases) for (auto& t : l.funded_terms)
            if (t.funding_operation_id == entry.funding_operation_id) {
                if (term) return StorageSettlementError::INVALID_PAYLOAD;
                term = &t; lease = &l;
            }
        if (!term) return StorageSettlementError::LEASE_NOT_FOUND;
        if (op.period < term->first_period || op.period >= term->end_period || op.period >= lease->end_period ||
            term->refunded_onboarding || term->refunded_locked) return StorageSettlementError::LEASE_NOT_ACTIVE;
        if (!state.accounts.contains(entry.payout_account)) return StorageSettlementError::PAYOUT_ACCOUNT_NOT_FOUND;
        if (entry.payout_account == lease->payer) return StorageSettlementError::SELF_PAYOUT;
        if (entry.slot >= lease->replicas || term->period_seconds != params.storage_settlement_period_seconds)
            return StorageSettlementError::INVALID_ASSIGNMENT;
        auto ledger = std::lower_bound(term->service_payments.begin(), term->service_payments.end(), entry,
            [&](const auto& a, const auto& b) { return key(a) < key(b); });
        const bool existing = ledger != term->service_payments.end() && key(*ledger) == key(entry);
        const auto prior_service = existing ? ledger->verified_unit_seconds : 0;
        const auto paid = existing ? ledger->paid : 0;
        if (entry.verified_unit_seconds <= prior_service) return StorageSettlementError::INVALID_PAYLOAD;
        const auto capacity = ComputeAcceptedStorageCapacity(*term, entry.slot, entry.storage_id,
            entry.payout_account, op.period + 1, lease->end_period);
        if (!capacity || entry.verified_unit_seconds > *capacity) return StorageSettlementError::INVALID_ASSIGNMENT;
        // Every interval capable of contributing to this key resolves a canonical
        // activation. A witness for a zero-length/superseded epoch is not useful.
        bool witnessed{false};
        for (size_t i = 0; i < term->assignments.size(); ++i) {
            const auto& epoch = term->assignments[i];
            const auto end = std::min({op.period + 1, lease->end_period, term->end_period,
                i + 1 < term->assignments.size() ? term->assignments[i + 1].effective_period : term->end_period});
            if (end <= epoch.effective_period || std::none_of(epoch.allocations.begin(), epoch.allocations.end(),
                [&](const auto& a) { return key(a) == key(entry); })) continue;
            if (!std::binary_search(op.activation_witnesses.begin(), op.activation_witnesses.end(), epoch.operation_id))
                return StorageSettlementError::INVALID_ASSIGNMENT;
            used_witnesses.insert(epoch.operation_id); witnessed = true;
        }
        if (!witnessed) return StorageSettlementError::INVALID_ASSIGNMENT;
        const AssignedStorageBudget budget{term->replica_share, term->initial_onboarding + term->initial_locked,
            term->contracted_unit_seconds};
        const auto due = ComputeAssignedStoragePayout(budget, entry.verified_unit_seconds, paid);
        if (!due || entry.amount != *due) return StorageSettlementError::PAYOUT_EXCEEDS_ESCROW;
        unsigned __int128 slot_service = entry.verified_unit_seconds;
        for (const auto& e : term->service_payments)
            if (e.slot == entry.slot && key(e) != key(entry)) slot_service += e.verified_unit_seconds;
        if (slot_service > term->contracted_unit_seconds) return StorageSettlementError::INVALID_ASSIGNMENT;
        if (term->paid_onboarding > term->initial_onboarding || term->paid_locked > term->initial_locked ||
            entry.amount > term->initial_onboarding - term->paid_onboarding + term->initial_locked - term->paid_locked)
            return StorageSettlementError::PAYOUT_EXCEEDS_ESCROW;
        const auto onboarding = std::min(entry.amount, term->initial_onboarding - term->paid_onboarding);
        const auto locked = entry.amount - onboarding;
        if (onboarding > lease->escrow_onboarding || locked > lease->escrow_locked)
            return StorageSettlementError::PAYOUT_EXCEEDS_ESCROW;
        auto& provider = state.accounts.at(entry.payout_account);
        if (onboarding > std::numeric_limits<uint64_t>::max() - provider.system_balance ||
            onboarding > std::numeric_limits<uint64_t>::max() - provider.onboarding_system_balance ||
            locked > std::numeric_limits<uint64_t>::max() - provider.balance) return StorageSettlementError::BALANCE_OVERFLOW;
        if (!existing) ledger = term->service_payments.insert(ledger,
            {entry.slot, entry.storage_id, entry.payout_account, 0, 0});
        const auto closed_capacity = ComputeAcceptedStorageCapacity(*term, entry.slot, entry.storage_id,
            entry.payout_account, term->assignments.back().effective_period, lease->end_period);
        if (!closed_capacity) return StorageSettlementError::INVALID_ASSIGNMENT;
        ledger->closed_epoch_capacity = *closed_capacity;
        ledger->verified_unit_seconds = entry.verified_unit_seconds; ledger->paid += entry.amount;
        term->paid_onboarding += onboarding; term->paid_locked += locked;
        lease->escrow_onboarding -= onboarding; lease->escrow_locked -= locked;
        provider.system_balance += onboarding; provider.onboarding_system_balance += onboarding; provider.balance += locked;
    }
    if (used_witnesses.size() != op.activation_witnesses.size()) return StorageSettlementError::INVALID_ASSIGNMENT;
    state.settlement.next_period = op.period + 1;
    state.settlement.next_period_start_utc = op.period_end_utc;
    for (auto& [id, lease] : state.leases) for (auto& term : lease.funded_terms) {
        if (std::min(term.end_period, lease.end_period) > state.settlement.next_period) continue;
        const auto onboarding = term.initial_onboarding - term.paid_onboarding - term.refunded_onboarding;
        const auto locked = term.initial_locked - term.paid_locked - term.refunded_locked;
        auto& payer = state.accounts.at(lease.payer);
        if (onboarding > lease.escrow_onboarding || locked > lease.escrow_locked ||
            onboarding > std::numeric_limits<uint64_t>::max() - payer.onboarding_system_balance ||
            onboarding > std::numeric_limits<uint64_t>::max() - payer.system_balance ||
            locked > std::numeric_limits<uint64_t>::max() - payer.system_balance - onboarding)
            return StorageSettlementError::BALANCE_OVERFLOW;
        payer.system_balance += onboarding + locked; payer.onboarding_system_balance += onboarding;
        lease.escrow_onboarding -= onboarding; lease.escrow_locked -= locked;
        term.refunded_onboarding += onboarding; term.refunded_locked += locked;
        for (auto& e : term.service_payments) {
            const auto capacity = ComputeAcceptedStorageCapacity(term, e.slot, e.storage_id, e.payout_account,
                state.settlement.next_period, lease.end_period);
            if (!capacity) return StorageSettlementError::INVALID_ASSIGNMENT;
            e.closed_epoch_capacity = *capacity;
        }
    }
    return StorageSettlementError::NONE;
}

} // namespace

StorageSettlementError CheckStorageSettlementInputs(const StorageSettlement& settlement,
    const CybouProtocolParameters& params, const CybouState& state,
    const Hash256& network_binding, uint64_t block_height, const Hash256& verified_parent_id)
{
    if (settlement.action != StorageSettlementAction::PAY) {
        auto candidate = state;
        return ApplyAssignmentInputs(settlement, network_binding, candidate, block_height, verified_parent_id, false);
    }
    auto candidate = state;
    const auto checked = ApplyPayInputs(settlement, params, candidate);
    if (checked != StorageSettlementError::NONE) return checked;
    return ValidateCybouState(candidate) == StateValidationError::NONE && TotalCybou(candidate) == TotalCybou(state)
        ? StorageSettlementError::NONE : StorageSettlementError::INVALID_PAYLOAD;
}

StorageSettlementError ApplyStorageSettlement(const StorageSettlement& settlement,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params,
    const IdentityHybridPublicKey& poa_key, CybouState& state,
    uint64_t block_height, const Hash256& verified_parent_id)
{
    const auto digest = ComputeStorageSettlementDigest(network_binding, settlement);
    if (!digest) return StorageSettlementError::INVALID_PAYLOAD;
    if (poa_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        !VerifyIdentityMessage(poa_key, settlement.poa_signature, *digest)) {
        return StorageSettlementError::INVALID_SIGNATURE;
    }
    if (settlement.action != StorageSettlementAction::PAY) {
        auto candidate = state;
        const auto checked = ApplyAssignmentInputs(settlement, network_binding, candidate, block_height, verified_parent_id, true);
        if (checked != StorageSettlementError::NONE) return checked;
        if (ValidateCybouState(candidate) != StateValidationError::NONE) return StorageSettlementError::INVALID_ASSIGNMENT;
        state = std::move(candidate);
        return StorageSettlementError::NONE;
    }
    auto candidate = state;
    const auto checked = ApplyPayInputs(settlement, params, candidate);
    if (checked != StorageSettlementError::NONE) return checked;
    if (ValidateCybouState(candidate) != StateValidationError::NONE || TotalCybou(candidate) != TotalCybou(state))
        return StorageSettlementError::INVALID_PAYLOAD;
    state = std::move(candidate);
    return StorageSettlementError::NONE;
}

} // namespace cybou
