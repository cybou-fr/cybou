// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief StorageLease и StorageSettlement: аренда хранения через escrow и PoA-выплаты providers.

#include <cybou/storage_lease.h>

#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>
#include <cybou/storage_economy.h>

#include <algorithm>
#include <limits>
#include <map>
#include <string_view>
#include <tuple>

namespace cybou {
namespace {

constexpr uint64_t RENT_DENOMINATOR{2048ULL * 86'400ULL};
constexpr size_t SETTLEMENT_HEADER_SIZE{8 + 8 + 4};
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

std::optional<uint64_t> RentCeil(const CybouProtocolParameters& params, uint32_t units, uint8_t replicas,
    uint64_t periods)
{
    if (units == 0 || replicas == 0 || periods == 0) return std::nullopt;
    uint64_t numerator{units};
    if (!CheckedMul(numerator, replicas, numerator) ||
        !CheckedMul(numerator, params.storage_rate_per_gib_day_replica, numerator) ||
        !CheckedMul(numerator, periods, numerator) ||
        !CheckedMul(numerator, params.storage_settlement_period_seconds, numerator)) return std::nullopt;
    return numerator / RENT_DENOMINATOR + (numerator % RENT_DENOMINATOR != 0);
}

std::optional<std::vector<unsigned char>> SerializeSettlementBody(const StorageSettlement& settlement)
{
    if (settlement.period_start_utc == 0 || settlement.entries.size() > MAX_STORAGE_SETTLEMENT_ENTRIES) {
        return std::nullopt;
    }
    std::vector<unsigned char> out;
    out.reserve(SETTLEMENT_HEADER_SIZE + settlement.entries.size() * STORAGE_SETTLEMENT_ENTRY_SIZE);
    Write64(out, settlement.period);
    Write64(out, settlement.period_start_utc);
    Write32(out, static_cast<uint32_t>(settlement.entries.size()));
    const StorageSettlementEntry* prior{nullptr};
    for (const auto& entry : settlement.entries) {
        // Строгий порядок по (publication, payout) делает выплаты уникальными и кодировку однозначной.
        if (entry.publication_id.IsNull() || entry.payout_account.IsNull() || entry.amount == 0 ||
            (prior && !(std::tie(prior->publication_id, prior->payout_account) <
                        std::tie(entry.publication_id, entry.payout_account)))) return std::nullopt;
        prior = &entry;
        out.insert(out.end(), entry.publication_id.begin(), entry.publication_id.end());
        out.insert(out.end(), entry.payout_account.Value().begin(), entry.payout_account.Value().end());
        Write64(out, entry.amount);
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

std::optional<uint64_t> ComputeStorageLeasePeriodCap(const CybouProtocolParameters& params, uint32_t units,
    uint8_t replicas)
{
    return RentCeil(params, units, replicas, 1);
}

void FundStorageLease(CybouState& state, const cybou::Hash256& publication_id, const AccountId& payer,
    const uint32_t units, const uint8_t replicas, const uint32_t periods, const uint64_t escrow)
{
    // Onboarding-происхождение уходит в escrow первым и сохраняется для выплат (DEC-281).
    const uint64_t onboarding = DebitSystemBalance(state.accounts.at(payer), escrow);
    if (auto existing = state.leases.find(publication_id); existing != state.leases.end()) {
        existing->second.end_period += periods;
        existing->second.escrow_onboarding += onboarding;
        existing->second.escrow_locked += escrow - onboarding;
        return;
    }
    const uint64_t first = state.settlement.next_period;
    state.leases.emplace(publication_id, StorageLeaseRecord{.payer = payer, .units = units, .replicas = replicas,
        .first_period = first, .end_period = first + periods, .escrow_onboarding = onboarding,
        .escrow_locked = escrow - onboarding});
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
        op.lease.periods, *escrow);
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
    if (bytes.size() < SETTLEMENT_HEADER_SIZE + POA_SIGNATURE_SIZE) return std::nullopt;
    StorageSettlement settlement{.period = Read64(bytes.subspan(0, 8)), .period_start_utc = Read64(bytes.subspan(8, 8))};
    const uint32_t count = Read32(bytes.subspan(16, 4));
    if (count > MAX_STORAGE_SETTLEMENT_ENTRIES ||
        bytes.size() != SETTLEMENT_HEADER_SIZE + size_t{count} * STORAGE_SETTLEMENT_ENTRY_SIZE + POA_SIGNATURE_SIZE) {
        return std::nullopt;
    }
    size_t offset{SETTLEMENT_HEADER_SIZE};
    settlement.entries.reserve(count);
    for (uint32_t i{0}; i < count; ++i) {
        StorageSettlementEntry entry;
        std::copy_n(bytes.begin() + offset, 32, entry.publication_id.begin());
        const auto payout = AccountId::FromBytes(bytes.subspan(offset + 32, 32));
        if (!payout) return std::nullopt;
        entry.payout_account = *payout;
        entry.amount = Read64(bytes.subspan(offset + 64, 8));
        settlement.entries.push_back(entry);
        offset += STORAGE_SETTLEMENT_ENTRY_SIZE;
    }
    std::copy_n(bytes.begin() + offset, 64, settlement.poa_signature.ed25519.begin());
    settlement.poa_signature.ml_dsa.assign(bytes.begin() + offset + 64, bytes.end());
    if (!SerializeSettlementBody(settlement)) return std::nullopt;
    return settlement;
}

StorageSettlementError ApplyStorageSettlement(const StorageSettlement& settlement,
    const cybou::Hash256& network_binding, const CybouProtocolParameters& params,
    const IdentityHybridPublicKey& poa_key, CybouState& state)
{
    const auto digest = ComputeStorageSettlementDigest(network_binding, settlement);
    if (!digest) return StorageSettlementError::INVALID_PAYLOAD;
    if (poa_key.purpose != IdentityKeyPurpose::POA_FINALIZER ||
        !VerifyIdentityMessage(poa_key, settlement.poa_signature, *digest)) {
        return StorageSettlementError::INVALID_SIGNATURE;
    }
    // Периоды строго непрерывны: replay или пропуск периода невозможны.
    if (settlement.period != state.settlement.next_period) return StorageSettlementError::WRONG_PERIOD;
    if (state.settlement.next_period_start_utc != 0 &&
        settlement.period_start_utc != state.settlement.next_period_start_utc) {
        return StorageSettlementError::WRONG_PERIOD_START;
    }
    if (settlement.period == std::numeric_limits<uint64_t>::max() ||
        settlement.period_start_utc > std::numeric_limits<uint64_t>::max() - params.storage_settlement_period_seconds) {
        return StorageSettlementError::INVALID_PAYLOAD;
    }

    // Сначала полная проверка всех выплат, затем применение: settlement атомарен.
    struct LeaseTotals { uint64_t amount{0}; uint32_t payouts{0}; };
    std::map<cybou::Hash256, LeaseTotals> totals;
    for (const auto& entry : settlement.entries) {
        const auto lease = state.leases.find(entry.publication_id);
        if (lease == state.leases.end()) return StorageSettlementError::LEASE_NOT_FOUND;
        if (settlement.period < lease->second.first_period || settlement.period >= lease->second.end_period) {
            return StorageSettlementError::LEASE_NOT_ACTIVE;
        }
        if (!state.accounts.contains(entry.payout_account)) return StorageSettlementError::PAYOUT_ACCOUNT_NOT_FOUND;
        // Плательщик никогда не оплачивает сам себя: это закрывает прямой self-dealing.
        if (entry.payout_account == lease->second.payer) return StorageSettlementError::SELF_PAYOUT;
        auto& total = totals[entry.publication_id];
        if (++total.payouts > lease->second.replicas) return StorageSettlementError::TOO_MANY_PAYOUTS;
        const auto cap = ComputeStorageLeasePeriodCap(params, lease->second.units, lease->second.replicas);
        if (!cap || entry.amount > *cap - std::min(*cap, total.amount) ||
            total.amount + entry.amount > lease->second.escrow_onboarding + lease->second.escrow_locked) {
            return StorageSettlementError::PAYOUT_EXCEEDS_ESCROW;
        }
        total.amount += entry.amount;
    }

    // Переводы внутри TotalCybou < 2^64 не могут переполнить ни один баланс.
    for (const auto& entry : settlement.entries) {
        auto& lease = state.leases.at(entry.publication_id);
        auto& provider = state.accounts.at(entry.payout_account);
        const uint64_t onboarding = std::min(lease.escrow_onboarding, entry.amount);
        lease.escrow_onboarding -= onboarding;
        lease.escrow_locked -= entry.amount - onboarding;
        provider.system_balance += onboarding;
        provider.onboarding_system_balance += onboarding;
        provider.balance += entry.amount - onboarding;
    }
    state.settlement.next_period = settlement.period + 1;
    state.settlement.next_period_start_utc = settlement.period_start_utc + params.storage_settlement_period_seconds;
    // Закончившиеся аренды возвращают остаток escrow в System Balance плательщика, с тем же происхождением.
    for (auto it = state.leases.begin(); it != state.leases.end();) {
        if (it->second.end_period > state.settlement.next_period) { ++it; continue; }
        auto& payer = state.accounts.at(it->second.payer);
        payer.system_balance += it->second.escrow_onboarding + it->second.escrow_locked;
        payer.onboarding_system_balance += it->second.escrow_onboarding;
        it = state.leases.erase(it);
    }
    return StorageSettlementError::NONE;
}

} // namespace cybou
