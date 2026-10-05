// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see COPYING.
// Rebuildable public coordinates. No recipient or decrypted content index.
#include <cybou/state_store.h>
#include <cybou/poa_finality.h>
#include <charconv>
#include <limits>
#include <stdexcept>

namespace cybou {
namespace {
const std::string HEAD{"cybou/events/head"};
const std::string OPS{"cybou/events/op/"};
const std::string KEM{"cybou/events/kem/"};
const std::string PUBLICATIONS{"cybou/events/publication/"};
std::string OrderedNumber(uint64_t number)
{
    std::string out(16, '0');
    constexpr char digits[]{"0123456789abcdef"};
    for (size_t i = 0; i < 16; ++i) { out[15 - i] = digits[number & 15]; number >>= 4; }
    return out;
}
std::string KemKey(const AccountId& account, uint64_t epoch)
{ return KEM + account.Value().GetHex() + '/' + OrderedNumber(epoch); }
std::string EncodeLocation(const FinalizedEventLocation& location)
{
    auto bytes = detail::SerializeLocalRecord(location.height);
    const auto index = detail::SerializeLocalRecord(location.operation_index);
    bytes.insert(bytes.end(), index.begin(), index.end());
    bytes.insert(bytes.end(), location.block_id.begin(), location.block_id.end());
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
std::optional<FinalizedEventLocation> DecodeLocation(const std::string& value)
{
    if (value.size() != 44) return std::nullopt;
    const auto bytes = std::span{reinterpret_cast<const unsigned char*>(value.data()), value.size()};
    FinalizedEventLocation location;
    if (!detail::DeserializeLocalRecord(bytes.first(8), location.height) ||
        !detail::DeserializeLocalRecord(bytes.subspan(8, 4), location.operation_index) ||
        !detail::DeserializeLocalRecord(bytes.subspan(12), location.block_id)) return std::nullopt;
    return location;
}
void WriteEvents(KVStore::Batch& batch, const FinalizedBlock& finalized, bool erase = false)
{
    const auto& block = finalized.block;
    const auto block_id = ComputeBlockId(block);
    bool publication{false};
    for (size_t i = 0; i < block.operations.size(); ++i) {
        const auto& operation = block.operations[i];
        const auto id = ComputeOperationId(operation);
        if (!id) throw std::runtime_error("cannot index invalid finalized operation");
        const auto value = EncodeLocation({block.height, static_cast<uint32_t>(i), block_id});
        if (erase) batch.Erase(OPS + id->GetHex());
        else batch.Write(OPS + id->GetHex(), value);
        std::optional<std::string> kem_key;
        if (const auto* create = std::get_if<AccountCreateOp>(&operation)) kem_key = KemKey(create->account_id, 0);
        else if (const auto* rotate = std::get_if<IdentityRotate>(&operation)) kem_key = KemKey(rotate->account_id, rotate->key_epoch);
        if (kem_key) {
            if (erase) batch.Erase(*kem_key);
            else batch.Write(*kem_key, value);
        }
        publication |= std::holds_alternative<AuthorizedRootPublication>(operation);
    }
    const auto key = PUBLICATIONS + OrderedNumber(block.height);
    // Erase even an absent row: canonical replacement may remove the only publication.
    if (erase || !publication) batch.Erase(key);
    else batch.Write(key, EncodeLocation({block.height, 0, block_id}));
}
} // namespace

void CybouStateStore::AppendFinalizedEvents(KVStore::Batch& batch, const FinalizedBlock& block,
    const FinalizedHead& previous, const FinalizedBlock* replaced) const
{
    if (replaced) WriteEvents(batch, *replaced, true);
    WriteEvents(batch, block);
    std::vector<unsigned char> marker;
    // An incomplete older database stays incomplete until a full rebuild succeeds.
    if (m_db.Read(HEAD, marker) && marker == detail::SerializeLocalRecord(previous))
        batch.Write(HEAD, detail::SerializeLocalRecord(FinalizedHead{ComputeBlockId(block.block), block.block.height}));
}

bool CybouStateStore::EnsureFinalizedEventsLocked(const bool force) const
{
    const auto head = GetFinalizedHead();
    if (!head) return false;
    std::vector<unsigned char> marker;
    if (!force && m_db.Read(HEAD, marker) && marker == detail::SerializeLocalRecord(*head)) return true;
    // Invalidate first. A crash during rebuild leaves the cache explicitly incomplete.
    m_db.Erase(HEAD);
    for (const auto& [prefix, width] : std::vector<std::pair<std::string, size_t>>{
             {OPS, 64}, {KEM, 81}, {PUBLICATIONS, 16}}) {
        KVStore::Batch erase;
        size_t count{0};
        m_db.ForEachStringPrefixRaw(prefix, prefix.size() + width, [&](const std::string& key, const std::string&) {
            erase.Erase(key);
            if (++count == 256) { m_db.WriteBatch(erase); erase = KVStore::Batch{}; count = 0; }
        });
        m_db.WriteBatch(erase);
    }
    Hash256 previous = m_network_genesis.GetGenesisAnchor();
    for (uint64_t height = 1; height <= head->height; ++height) {
        const auto block = GetBlockAtHeight(height);
        if (!block || block->block.parent_block_id != previous ||
            !VerifyPoaCertificateForBlock(block->certificate, m_network_genesis.GetPoaPublicKey(), m_network_binding, block->block)) return false;
        KVStore::Batch batch;
        WriteEvents(batch, *block);
        m_db.WriteBatch(batch);
        previous = ComputeBlockId(block->block);
        if (height == head->height) break;
    }
    if (previous != head->block_id) return false;
    m_db.Write(HEAD, detail::SerializeLocalRecord(*head), true);
    return true;
}

bool CybouStateStore::RebuildFinalizedEventIndex() const
{
    std::lock_guard lock{m_snapshot_mutex};
    return EnsureFinalizedEventsLocked(true);
}

bool CybouStateStore::ValidateFinalizedEvent(const FinalizedEventLocation& location) const
{
    const auto head = GetFinalizedHead();
    const auto block = GetBlockAtHeight(location.height);
    if (!head || location.height == 0 || location.height > head->height || !block ||
        ComputeBlockId(block->block) != location.block_id || location.operation_index >= block->block.operations.size()) return false;
    Hash256 parent = m_network_genesis.GetGenesisAnchor();
    if (location.height > 1 && !m_db.Read("cybou/block-height/" + std::to_string(location.height - 1), parent)) return false;
    return block->block.parent_block_id == parent && VerifyPoaCertificateForBlock(block->certificate,
        m_network_genesis.GetPoaPublicKey(), m_network_binding, block->block);
}

FinalizedEventLookup CybouStateStore::FindIndexedOperation(const Hash256& id) const
{
    std::lock_guard lock{m_snapshot_mutex};
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!EnsureFinalizedEventsLocked(attempt != 0)) return {};
        std::string row;
        if (m_db.Read(OPS + id.GetHex(), row)) {
            const auto location = DecodeLocation(row);
            if (location && ValidateFinalizedEvent(*location)) {
                const auto block = GetBlockAtHeight(location->height);
                if (block && ComputeOperationId(block->block.operations[location->operation_index]) == id) return {true, location};
            }
        } else if (!m_db.Exists("cybou/operation/" + id.GetHex())) return {true, std::nullopt};
    }
    return {};
}

FinalizedEventLookup CybouStateStore::FindIndexedKem(const AccountId& account, const uint64_t epoch) const
{
    std::lock_guard lock{m_snapshot_mutex};
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!EnsureFinalizedEventsLocked(attempt != 0)) return {};
        std::string row;
        if (!m_db.Read(KemKey(account, epoch), row)) { if (attempt) return {true, std::nullopt}; continue; }
        const auto location = DecodeLocation(row);
        if (!location || !ValidateFinalizedEvent(*location)) continue;
        const auto block = GetBlockAtHeight(location->height);
        const auto& operation = block->block.operations[location->operation_index];
        const auto* create = std::get_if<AccountCreateOp>(&operation);
        const auto* rotate = std::get_if<IdentityRotate>(&operation);
        if ((create && epoch == 0 && create->account_id == account) ||
            (rotate && rotate->key_epoch == epoch && rotate->account_id == account)) return {true, location};
    }
    return {};
}

std::optional<FinalizedPublicationScan> CybouStateStore::ScanPublicationHeights(
    const uint64_t after, const uint64_t through, const uint64_t max_blocks) const
{
    std::lock_guard lock{m_snapshot_mutex};
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!EnsureFinalizedEventsLocked(attempt != 0)) return std::nullopt;
        const auto head = GetFinalizedHead();
        if (!head || through > head->height) return std::nullopt;
        FinalizedPublicationScan result{{}, after};
        if (after >= through || max_blocks == 0) return result;
        bool valid{true}, limited{false};
        const auto limit = std::min<uint64_t>(max_blocks, 256);
        m_db.ForEachStringRange(PUBLICATIONS + OrderedNumber(after + 1), PUBLICATIONS + OrderedNumber(through),
            [&](const std::string& key, const std::string& value) {
                if (key.size() != PUBLICATIONS.size() + 16) { valid = false; return false; }
                const auto location = DecodeLocation(value);
                uint64_t height{0};
                const auto begin = key.data() + PUBLICATIONS.size();
                const auto parsed = std::from_chars(begin, key.data() + key.size(), height, 16);
                if (parsed.ec != std::errc{} || parsed.ptr != key.data() + key.size() || !location ||
                    location->height != height || !ValidateFinalizedEvent(*location)) { valid = false; return false; }
                const auto block = GetBlockAtHeight(height);
                if (!std::any_of(block->block.operations.begin(), block->block.operations.end(), [](const auto& operation) {
                    return std::holds_alternative<AuthorizedRootPublication>(operation);
                })) { valid = false; return false; }
                result.heights.push_back(height);
                result.scanned_height = height;
                limited = result.heights.size() >= limit;
                return !limited;
            });
        if (!valid) continue;
        if (!limited) result.scanned_height = through;
        return result;
    }
    return std::nullopt;
}
} // namespace cybou
