// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/authority.h>

#include <cybou/block.h>
#include <cybou/chunk_id.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_params.h>

#include <algorithm>
#include <bit>
#include <fstream>
#include <iterator>
#include <type_traits>
#include <variant>

namespace cybou {

std::uint32_t AuthorityTier(const std::uint64_t effective, const std::uint32_t max_tier)
{
    // floor(log2(effective + 1)); saturates so effective == UINT64_MAX stays defined.
    const std::uint64_t value = SaturatingAdd(effective, 1);
    const auto tier = static_cast<std::uint32_t>(63 - std::countl_zero(value));
    return std::min(tier, max_tier);
}

AuthorityBudgets AuthorityBudgetsForTier(const std::uint32_t tier, const AuthorityPolicy& policy)
{
    const auto budget = [tier](std::uint64_t base, std::uint64_t per_tier, std::uint64_t ceiling) {
        return std::min(SaturatingAdd(base, SaturatingMul(per_tier, tier)), ceiling);
    };
    return {
        .protocol_operations_per_epoch = budget(policy.protocol_base, policy.protocol_per_tier, policy.protocol_ceiling),
        .storage_bytes = budget(policy.storage_base, policy.storage_per_tier, policy.storage_ceiling),
        .bandwidth_bytes_per_epoch = budget(policy.bandwidth_base, policy.bandwidth_per_tier, policy.bandwidth_ceiling),
    };
}

std::optional<AccountId> AuthorizingAccount(const ProtocolOperation& operation)
{
    return std::visit([](const auto& op) -> std::optional<AccountId> {
        using T = std::decay_t<decltype(op)>;
        if constexpr (std::is_same_v<T, AccountCreateOp>) {
            // Creation establishes the Identity; it is not qualifying activity.
            return std::nullopt;
        } else if constexpr (std::is_same_v<T, IdentityRotate>) {
            return op.account_id;
        } else {
            return op.authorization.account_id;
        }
    }, operation);
}

namespace {

constexpr std::array<unsigned char, 6> CHECKPOINT_MAGIC{'C', 'Y', 'A', 'I', 'X', 1};
/** Bounded read: a corrupt or hostile file can never force a huge allocation. */
constexpr std::uintmax_t MAX_CHECKPOINT_BYTES{64ULL << 20};

void Put64(std::vector<unsigned char>& out, std::uint64_t value)
{
    for (int i{0}; i < 8; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

struct CheckpointReader {
    std::span<const unsigned char> bytes;
    std::size_t offset{0};
    bool Take(std::span<unsigned char> out)
    {
        if (bytes.size() - offset < out.size()) return false;
        std::copy_n(bytes.begin() + static_cast<std::ptrdiff_t>(offset), out.size(), out.begin());
        offset += out.size();
        return true;
    }
    std::optional<std::uint64_t> U64()
    {
        std::array<unsigned char, 8> raw{};
        if (!Take(raw)) return std::nullopt;
        std::uint64_t value{0};
        for (int i{0}; i < 8; ++i) value |= std::uint64_t{raw[i]} << (8 * i);
        return value;
    }
};

} // namespace

AuthorityIndex::AuthorityIndex(CybouNodeRuntime& runtime, AuthorityPolicy policy)
    : m_runtime{runtime}, m_policy{policy}
{
}

AuthorityIndex::AuthorityIndex(CybouNodeRuntime& runtime, std::filesystem::path checkpoint, AuthorityPolicy policy)
    : m_runtime{runtime}, m_policy{policy}, m_checkpoint{std::move(checkpoint)}
{
    std::lock_guard lock{m_mutex};
    if (!LoadCheckpoint()) {
        m_height = 0;
        m_tallies.clear();
    }
}

std::array<unsigned char, 32> AuthorityIndex::PolicyFingerprint() const
{
    // A different policy means different tallies: never reuse them.
    std::vector<unsigned char> bytes{'C', 'Y', 'A', 'P', 1};
    for (const auto value : {m_policy.activity_cap_per_epoch, std::uint64_t{m_policy.max_tier},
             m_policy.protocol_base, m_policy.protocol_per_tier, m_policy.protocol_ceiling,
             m_policy.storage_base, m_policy.storage_per_tier, m_policy.storage_ceiling,
             m_policy.bandwidth_base, m_policy.bandwidth_per_tier, m_policy.bandwidth_ceiling}) {
        Put64(bytes, value);
    }
    return ComputeBlake3Digest(bytes);
}

bool AuthorityIndex::LoadCheckpoint()
{
    if (m_checkpoint.empty()) return false;
    std::error_code ec;
    const auto size = std::filesystem::file_size(m_checkpoint, ec);
    if (ec || size == 0 || size > MAX_CHECKPOINT_BYTES) return false;
    std::ifstream in{m_checkpoint, std::ios::binary};
    std::vector<unsigned char> bytes{std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
    if (bytes.size() != size) return false;
    CheckpointReader reader{bytes};
    std::array<unsigned char, CHECKPOINT_MAGIC.size()> magic{};
    std::array<unsigned char, 32> network{};
    std::array<unsigned char, 32> policy{};
    std::array<unsigned char, 32> block_id{};
    if (!reader.Take(magic) || magic != CHECKPOINT_MAGIC || !reader.Take(network) || !reader.Take(policy)) return false;
    const auto& network_id = m_runtime.GetNetworkId();
    if (!std::equal(network.begin(), network.end(), network_id.begin()) || policy != PolicyFingerprint()) return false;
    const auto height = reader.U64();
    if (!height || !reader.Take(block_id)) return false;
    // The checkpoint must still describe this node's finalized history.
    if (*height > 0) {
        const auto block = m_runtime.GetBlockAtHeight(*height);
        if (!block) return false;
        const auto id = ComputeBlockId(block->block);
        if (!std::equal(block_id.begin(), block_id.end(), id.begin())) return false;
    }
    const auto count = reader.U64();
    if (!count || *count > (bytes.size() - reader.offset) / (AccountId::SIZE + 32)) return false;
    std::map<AccountId, Tally> tallies;
    for (std::uint64_t i{0}; i < *count; ++i) {
        std::array<unsigned char, AccountId::SIZE> account_bytes{};
        if (!reader.Take(account_bytes)) return false;
        const auto account = AccountId::FromBytes(account_bytes);
        const auto activity = reader.U64();
        const auto epoch = reader.U64();
        const auto epoch_count = reader.U64();
        const auto contribution = reader.U64();
        if (!account || !activity || !epoch || !epoch_count || !contribution) return false;
        tallies[*account] = Tally{*activity, *epoch, *epoch_count, *contribution};
    }
    if (reader.offset != bytes.size()) return false;
    m_height = *height;
    m_tallies = std::move(tallies);
    return true;
}

bool AuthorityIndex::SaveCheckpoint() const
{
    if (m_checkpoint.empty()) return true;
    std::vector<unsigned char> out(CHECKPOINT_MAGIC.begin(), CHECKPOINT_MAGIC.end());
    const auto& network_id = m_runtime.GetNetworkId();
    out.insert(out.end(), network_id.begin(), network_id.end());
    const auto policy = PolicyFingerprint();
    out.insert(out.end(), policy.begin(), policy.end());
    Put64(out, m_height);
    uint256 block_id;
    if (m_height > 0) {
        const auto block = m_runtime.GetBlockAtHeight(m_height);
        if (!block) return false;
        block_id = ComputeBlockId(block->block);
    }
    out.insert(out.end(), block_id.begin(), block_id.end());
    Put64(out, m_tallies.size());
    for (const auto& [account, tally] : m_tallies) {
        const auto& value = account.Value();
        out.insert(out.end(), value.begin(), value.begin() + AccountId::SIZE);
        Put64(out, tally.activity);
        Put64(out, tally.epoch);
        Put64(out, tally.epoch_count);
        Put64(out, tally.system_contribution);
    }
    // Atomic replace: a crash leaves the old checkpoint or the new one, never half.
    std::error_code ec;
    std::filesystem::create_directories(m_checkpoint.parent_path(), ec);
    auto temp = m_checkpoint;
    temp += ".tmp";
    {
        std::ofstream file{temp, std::ios::binary | std::ios::trunc};
        file.write(reinterpret_cast<const char*>(out.data()), static_cast<std::streamsize>(out.size()));
        if (!file) return false;
    }
    std::filesystem::rename(temp, m_checkpoint, ec);
    return !ec;
}

std::uint64_t AuthorityIndex::ScannedHeight() const
{
    std::lock_guard lock{m_mutex};
    return m_height;
}

std::uint64_t AuthorityIndex::Sync(const std::uint64_t max_blocks)
{
    std::lock_guard lock{m_mutex};
    const auto start_height = m_height;
    const auto finalized = m_runtime.GetFinalizedHeight().value_or(0);
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    for (std::uint64_t scanned{0}; scanned < max_blocks && m_height < finalized; ++scanned) {
        const auto block = m_runtime.GetBlockAtHeight(m_height + 1);
        if (!block) break;
        const auto epoch = EpochForHeight(block->block.height, params);
        for (const auto& operation : block->block.operations) {
            const auto account = AuthorizingAccount(operation);
            if (!account) continue;
            auto& tally = m_tallies[*account];
            if (tally.epoch != epoch) {
                tally.epoch = epoch;
                tally.epoch_count = 0;
            }
            // Activity: +1 per finalized Identity operation, capped per epoch.
            if (tally.epoch_count < m_policy.activity_cap_per_epoch) {
                ++tally.epoch_count;
                tally.activity = SaturatingAdd(tally.activity, 1);
            }
            // Voluntary Balance -> System Balance lock: one-time contribution.
            if (const auto* lock_op = std::get_if<AuthorizedSystemLock>(&operation)) {
                tally.system_contribution = SaturatingAdd(tally.system_contribution, lock_op->lock.amount);
            }
        }
        ++m_height;
    }
    // Losing the cache only costs a rescan, so a failed write is not an error.
    if (m_height != start_height) (void)SaveCheckpoint();
    return m_height;
}

std::optional<AuthorityRecord> AuthorityIndex::Get(const AccountId& account)
{
    std::lock_guard lock{m_mutex};
    const auto state = m_runtime.GetAccountState(account);
    if (!state) return std::nullopt;
    const auto& params = m_runtime.GetNetworkDefinition().protocol_parameters;
    AuthorityRecord record;
    record.account_id = account;
    record.creation_epoch = state->creation_epoch;
    record.current_epoch = EpochForHeight(m_height, params);
    // Age: completed epochs of Identity lifetime.
    record.age = record.current_epoch > record.creation_epoch ? record.current_epoch - record.creation_epoch : 0;
    if (const auto tally = m_tallies.find(account); tally != m_tallies.end()) {
        record.activity = tally->second.activity;
        record.system_contribution = tally->second.system_contribution;
    }
    record.earned = SaturatingAdd(SaturatingAdd(SaturatingAdd(record.age, record.activity),
        SaturatingAdd(record.system_contribution, record.liveness)), record.storage);
    record.effective = record.earned > record.penalty_debt ? record.earned - record.penalty_debt : 0;
    record.tier = AuthorityTier(record.effective, m_policy.max_tier);
    record.budgets = AuthorityBudgetsForTier(record.tier, m_policy);
    record.enforced = false;
    return record;
}

} // namespace cybou
