// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/application_service.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/identity_kem.h>
#include <cybou/keystore.h>
#include <cybou/node_runtime.h>
#include <cybou/protocol_operation.h>
#include <cybou/storage_service.h>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <variant>

namespace cybou {
namespace {

constexpr std::array<unsigned char, 5> ACCESSIBLE_MAGIC{'C', 'Y', 'A', 'P', 1};
constexpr std::array<unsigned char, 5> MAIL_MAGIC{'C', 'Y', 'M', 'L', 1};
constexpr std::array<unsigned char, 5> FILE_MAGIC{'C', 'Y', 'F', 'R', 1};
constexpr std::string_view SCAN_KEY{"app/scan-height"};
constexpr std::string_view UNAVAILABLE_KEY{"app/unavailable"};
constexpr std::string_view MAIL_INDEX_KEY{"mail/index"};
constexpr std::string_view FILES_INDEX_KEY{"files/index"};
constexpr std::string_view BRIDGE_INDEX_KEY{"recovery/index"};
constexpr std::string_view RECOVERED_EPOCHS_KEY{"recovery/recovered-epochs"};
/** Private metadata roots only; application content lives in child trees. */
constexpr std::uint64_t MAX_ROOT_PLAINTEXT_BYTES{0};

template <typename Bytes>
std::string Hex(const Bytes& bytes)
{
    static constexpr char DIGITS[]{"0123456789abcdef"};
    std::string out;
    out.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        out.push_back(DIGITS[byte >> 4]);
        out.push_back(DIGITS[byte & 0x0f]);
    }
    return out;
}

std::string AccessibleKey(const uint256& id) { return "app/pub/" + id.GetHex(); }
std::string MailKey(const PrivateItemId& id) { return "mail/msg/" + Hex(id); }
std::string FileKey(const PrivateItemId& id) { return "files/item/" + Hex(id); }
std::string BridgeKey(const uint256& id) { return "recovery/bridge/" + id.GetHex(); }

class Writer {
public:
    template <std::size_t N>
    void Bytes(const std::array<unsigned char, N>& value) { m_out.insert(m_out.end(), value.begin(), value.end()); }
    void Bytes(std::span<const unsigned char> value) { m_out.insert(m_out.end(), value.begin(), value.end()); }
    void U8(std::uint8_t value) { m_out.push_back(value); }
    void U32(std::uint32_t value) { for (unsigned i{0}; i < 4; ++i) m_out.push_back(static_cast<unsigned char>(value >> (8 * i))); }
    void U64(std::uint64_t value) { for (unsigned i{0}; i < 8; ++i) m_out.push_back(static_cast<unsigned char>(value >> (8 * i))); }
    std::vector<unsigned char>& Out() { return m_out; }

private:
    std::vector<unsigned char> m_out;
};

class Reader {
public:
    explicit Reader(std::span<const unsigned char> bytes) : m_bytes{bytes} {}
    bool Bytes(std::span<unsigned char> out)
    {
        if (m_bytes.size() - m_offset < out.size()) return false;
        std::copy_n(m_bytes.begin() + m_offset, out.size(), out.begin());
        m_offset += out.size();
        return true;
    }
    std::optional<std::uint8_t> U8()
    {
        std::array<unsigned char, 1> b{};
        return Bytes(b) ? std::optional<std::uint8_t>{b[0]} : std::nullopt;
    }
    std::optional<std::uint32_t> U32()
    {
        std::array<unsigned char, 4> b{};
        if (!Bytes(b)) return std::nullopt;
        std::uint32_t v{0};
        for (unsigned i{0}; i < 4; ++i) v |= std::uint32_t{b[i]} << (8 * i);
        return v;
    }
    std::optional<std::uint64_t> U64()
    {
        std::array<unsigned char, 8> b{};
        if (!Bytes(b)) return std::nullopt;
        std::uint64_t v{0};
        for (unsigned i{0}; i < 8; ++i) v |= std::uint64_t{b[i]} << (8 * i);
        return v;
    }
    std::span<const unsigned char> Rest() const { return m_bytes.subspan(m_offset); }
    bool Done() const { return m_offset == m_bytes.size(); }

private:
    std::span<const unsigned char> m_bytes;
    std::size_t m_offset{0};
};

template <std::size_t N>
bool Magic(Reader& in, const std::array<unsigned char, N>& magic)
{
    std::array<unsigned char, N> read{};
    return in.Bytes(read) && read == magic;
}

std::optional<AccountId> ReadAccount(Reader& in)
{
    std::array<unsigned char, 32> bytes{};
    if (!in.Bytes(bytes)) return std::nullopt;
    return AccountId::FromBytes(bytes);
}

bool ReadUint256(Reader& in, uint256& out) { return in.Bytes(std::span{out.begin(), 32}); }

/** Fixed-width 32-byte ID lists used as indexes; the store itself is not enumerable. */
template <typename Id>
std::vector<Id> ReadIds(const PrivateApplicationStore& db, std::string_view key)
{
    std::vector<Id> ids;
    const auto encoded = db.Get(key);
    if (!encoded || encoded->size() % 32 != 0) return ids;
    for (std::size_t offset{0}; offset < encoded->size(); offset += 32) {
        Id id{};
        std::copy_n(encoded->begin() + offset, 32, id.begin());
        ids.push_back(id);
    }
    return ids;
}

template <typename Id>
bool WriteIds(PrivateApplicationStore& db, std::string_view key, const std::vector<Id>& ids)
{
    std::vector<unsigned char> out;
    out.reserve(ids.size() * 32);
    for (const auto& id : ids) out.insert(out.end(), id.begin(), id.end());
    return db.Put(key, out);
}

template <typename Id>
bool AddId(PrivateApplicationStore& db, std::string_view key, const Id& id)
{
    auto ids = ReadIds<Id>(db, key);
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) return true;
    ids.push_back(id);
    return WriteIds(db, key, ids);
}

template <typename Id>
bool RemoveId(PrivateApplicationStore& db, std::string_view key, const Id& id)
{
    auto ids = ReadIds<Id>(db, key);
    const auto size = ids.size();
    std::erase(ids, id);
    return ids.size() == size || WriteIds(db, key, ids);
}

} // namespace

PrivateItemId FilesTrashParent()
{
    PrivateItemId trash{};
    trash.fill(0xff);
    return trash;
}

struct ApplicationService::Accessible {
    AccessibleRootState state{AccessibleRootState::DISCOVERED};
    std::uint64_t height{0};
    std::uint32_t operation_index{0};
    AccountId sender;
    std::uint64_t sender_nonce{0};
    std::uint64_t sender_key_epoch{0};
    ChunkId root_chunk_id{};
    ContentKey content_key{};

    ~Accessible() { crypto::CleanseMemory(content_key.data(), content_key.size()); }
    Accessible() = default;
    Accessible(const Accessible&) = default;
    Accessible& operator=(const Accessible&) = default;
};

ApplicationService::ApplicationService(CybouNodeRuntime& runtime, CybouKeyStore& identity,
    PrivateApplicationStore& application_db, StorageService& storage)
    : m_runtime{runtime}, m_identity{identity}, m_application_db{application_db}, m_storage{storage}
{
}

/* ---- persistence ---- */

std::optional<ApplicationService::Accessible> ApplicationService::LoadAccessible(const uint256& id) const
{
    const auto encoded = m_application_db.Get(AccessibleKey(id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    Accessible record;
    const auto state = Magic(in, ACCESSIBLE_MAGIC) ? in.U8() : std::nullopt;
    const auto height = in.U64();
    const auto index = in.U32();
    const auto sender = ReadAccount(in);
    const auto nonce = in.U64();
    const auto epoch = in.U64();
    if (!state || *state < 1 || *state > 4 || !height || !index || !sender || !nonce || !epoch ||
        !in.Bytes(record.root_chunk_id) || !in.Bytes(record.content_key) || !in.Done()) return std::nullopt;
    record.state = static_cast<AccessibleRootState>(*state);
    record.height = *height;
    record.operation_index = *index;
    record.sender = *sender;
    record.sender_nonce = *nonce;
    record.sender_key_epoch = *epoch;
    return record;
}

bool ApplicationService::SaveAccessible(const uint256& id, const Accessible& record)
{
    Writer out;
    out.Bytes(ACCESSIBLE_MAGIC);
    out.U8(static_cast<std::uint8_t>(record.state));
    out.U64(record.height);
    out.U32(record.operation_index);
    out.Bytes(record.sender.Value());
    out.U64(record.sender_nonce);
    out.U64(record.sender_key_epoch);
    out.Bytes(record.root_chunk_id);
    out.Bytes(record.content_key);
    const bool saved = m_application_db.Put(AccessibleKey(id), out.Out());
    crypto::CleanseMemory(out.Out().data(), out.Out().size());
    return saved;
}

std::optional<MailRecord> ApplicationService::LoadMail(const PrivateItemId& id) const
{
    const auto encoded = m_application_db.Get(MailKey(id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    MailRecord record;
    if (!Magic(in, MAIL_MAGIC) || !ReadUint256(in, record.operation_id)) return std::nullopt;
    const auto height = in.U64();
    const auto index = in.U32();
    const auto sender = ReadAccount(in);
    const auto outgoing = in.U8();
    const auto folder = in.U8();
    const auto read = in.U8();
    const auto starred = in.U8();
    if (!height || !index || !sender || !outgoing || !folder || *folder < 1 || *folder > 4 || !read || !starred) {
        return std::nullopt;
    }
    const auto document = DecodePrivateApplicationDocument(in.Rest());
    if (!document || !std::holds_alternative<MailMessage>(*document)) return std::nullopt;
    record.finalized_height = *height;
    record.operation_index = *index;
    record.sender = *sender;
    record.outgoing = *outgoing != 0;
    record.folder = static_cast<MailFolder>(*folder);
    record.read = *read != 0;
    record.starred = *starred != 0;
    record.message = std::get<MailMessage>(*document);
    if (record.message.message_id != id) return std::nullopt;
    return record;
}

bool ApplicationService::SaveMail(const MailRecord& record)
{
    const auto document = EncodePrivateApplicationDocument(record.message);
    if (!document) return false;
    Writer out;
    out.Bytes(MAIL_MAGIC);
    out.Bytes(std::span<const unsigned char>{record.operation_id.begin(), 32});
    out.U64(record.finalized_height);
    out.U32(record.operation_index);
    out.Bytes(record.sender.Value());
    out.U8(record.outgoing ? 1 : 0);
    out.U8(static_cast<std::uint8_t>(record.folder));
    out.U8(record.read ? 1 : 0);
    out.U8(record.starred ? 1 : 0);
    out.Bytes(*document);
    return m_application_db.Put(MailKey(record.message.message_id), out.Out()) &&
        AddId(m_application_db, MAIL_INDEX_KEY, record.message.message_id);
}

std::optional<FileRecord> ApplicationService::LoadFile(const PrivateItemId& id) const
{
    const auto encoded = m_application_db.Get(FileKey(id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    FileRecord record;
    PrivateItemId stored_id{};
    if (!Magic(in, FILE_MAGIC) || !ReadUint256(in, record.operation_id) || !in.Bytes(stored_id) ||
        stored_id != id) return std::nullopt;
    const auto height = in.U64();
    const auto index = in.U32();
    const auto mutation = in.U32();
    const auto deleted = in.U8();
    if (!height || !index || !mutation || !deleted) return std::nullopt;
    record.order = {*height, *index, *mutation};
    record.deleted = *deleted != 0;
    record.item.item_id = id;
    if (record.deleted) return in.Done() ? std::optional{record} : std::nullopt;
    const auto document = DecodePrivateApplicationDocument(in.Rest());
    if (!document || !std::holds_alternative<FilesMutationBatch>(*document)) return std::nullopt;
    const auto& batch = std::get<FilesMutationBatch>(*document);
    if (batch.mutations.size() != 1 || !batch.mutations.front().item ||
        batch.mutations.front().item->item_id != id) return std::nullopt;
    record.item = *batch.mutations.front().item;
    return record;
}

bool ApplicationService::SaveFile(const FileRecord& record)
{
    Writer out;
    out.Bytes(FILE_MAGIC);
    out.Bytes(std::span<const unsigned char>{record.operation_id.begin(), 32});
    out.Bytes(record.item.item_id);
    out.U64(record.order.height);
    out.U32(record.order.operation_index);
    out.U32(record.order.mutation_index);
    out.U8(record.deleted ? 1 : 0);
    if (!record.deleted) {
        FilesMutationBatch single;
        single.mutations.push_back({FileMutationKind::UPSERT_ITEM, record.item.item_id, record.item});
        const auto document = EncodePrivateApplicationDocument(single);
        if (!document) return false;
        out.Bytes(*document);
    }
    return m_application_db.Put(FileKey(record.item.item_id), out.Out()) &&
        AddId(m_application_db, FILES_INDEX_KEY, record.item.item_id);
}

std::uint64_t ApplicationService::Checkpoint() const
{
    const auto encoded = m_application_db.Get(SCAN_KEY);
    if (!encoded) return 0;
    Reader in{*encoded};
    const auto height = in.U64();
    return height && in.Done() ? *height : 0;
}

/* ---- scanning ---- */

ApplicationScanProgress ApplicationService::Scan(const std::uint64_t max_blocks)
{
    std::lock_guard lock{m_mutex};
    ApplicationScanProgress progress;
    if (!m_application_db.IsUnlocked()) return progress;
    progress.finalized_height = m_runtime.GetFinalizedHeight().value_or(0);
    std::uint64_t height = Checkpoint();
    // Retry roots that were unavailable earlier; later blocks never wait for them.
    for (const auto& operation_id : ReadIds<uint256>(m_application_db, UNAVAILABLE_KEY)) {
        auto accessible = LoadAccessible(operation_id);
        if (!accessible) {
            RemoveId(m_application_db, UNAVAILABLE_KEY, operation_id);
            continue;
        }
        if (Index(operation_id, *accessible) != AccessibleRootState::TEMPORARILY_UNAVAILABLE) {
            RemoveId(m_application_db, UNAVAILABLE_KEY, operation_id);
        }
    }
    // The current KEM epoch comes from canonical state, never from local data.
    const auto me = m_identity.GetAccountId();
    const auto loaded = m_runtime.GetStore().LoadState();
    const auto* identity = me && loaded && loaded.state ? loaded.state->identities.Find(*me) : nullptr;
    if (!identity) {
        progress.scanned_height = height;
        return progress;
    }
    const std::uint64_t my_key_epoch = identity->key_epoch;
    // Historical KEM keys live only in memory; bring them back before scanning.
    if (ImportBridgeSeeds(*me, my_key_epoch)) height = 0;
    for (int pass{0}; pass < 2; ++pass) {
        for (std::uint64_t scanned{0}; scanned < max_blocks && height < progress.finalized_height; ++scanned) {
            if (!ProcessBlock(height + 1, my_key_epoch)) break;
            ++height;
            Writer out;
            out.U64(height);
            if (!m_application_db.Put(SCAN_KEY, out.Out())) break;
        }
        // A bridge found during this scan may open older publications: rescan once.
        if (height < progress.finalized_height || !ImportBridgeSeeds(*me, my_key_epoch)) break;
        height = 0;
    }
    if (height == 0) {
        Writer out;
        out.U64(0);
        m_application_db.Put(SCAN_KEY, out.Out());
    }
    progress.scanned_height = Checkpoint();
    progress.unavailable_roots = static_cast<std::uint32_t>(ReadIds<uint256>(m_application_db, UNAVAILABLE_KEY).size());
    return progress;
}

ApplicationScanProgress ApplicationService::Progress()
{
    std::lock_guard lock{m_mutex};
    return {.scanned_height = Checkpoint(),
        .finalized_height = m_runtime.GetFinalizedHeight().value_or(0),
        .unavailable_roots = static_cast<std::uint32_t>(ReadIds<uint256>(m_application_db, UNAVAILABLE_KEY).size())};
}

bool ApplicationService::ProcessBlock(const std::uint64_t height, const std::uint64_t my_key_epoch)
{
    const auto block = m_runtime.GetBlockAtHeight(height);
    if (!block) return false;
    for (std::uint32_t index{0}; index < block->block.operations.size(); ++index) {
        const auto& operation = block->block.operations[index];
        const auto* publication = std::get_if<AuthorizedRootPublication>(&operation);
        if (!publication) continue;
        const auto operation_id = ComputeOperationId(operation);
        if (!operation_id) return false;
        if (!ProcessPublication(height, index, *operation_id, *publication, my_key_epoch)) return false;
    }
    return true;
}

bool ApplicationService::ProcessPublication(const std::uint64_t height, const std::uint32_t index,
    const uint256& operation_id, const AuthorizedRootPublication& publication, const std::uint64_t my_key_epoch)
{
    // Idempotent: an already recorded publication is never processed twice.
    if (m_application_db.Has(AccessibleKey(operation_id))) return true;
    const auto network = std::span<const unsigned char, 32>{m_runtime.GetNetworkId().begin(), 32};
    const auto& authorization = publication.authorization;
    for (const auto& capsule : publication.publication.recipient_capsules) {
        if (!m_identity.HasKemSeedForEpoch(capsule.key_epoch, my_key_epoch)) continue;
        auto key = m_identity.OpenRootCapsule(network, authorization.account_id, authorization.nonce,
            authorization.key_epoch, publication.publication.root_chunk_id, capsule, my_key_epoch);
        if (!key) continue;
        Accessible accessible;
        accessible.height = height;
        accessible.operation_index = index;
        accessible.sender = authorization.account_id;
        accessible.sender_nonce = authorization.nonce;
        accessible.sender_key_epoch = authorization.key_epoch;
        accessible.root_chunk_id = publication.publication.root_chunk_id;
        accessible.content_key = *key;
        crypto::CleanseMemory(key->data(), key->size());
        // Positive record first, so a crash before indexing resumes as a retry.
        if (!SaveAccessible(operation_id, accessible)) return false;
        Index(operation_id, accessible);
        return true;
    }
    // Not for this Identity: nothing is recorded.
    return true;
}

AccessibleRootState ApplicationService::Index(const uint256& operation_id, Accessible& accessible)
{
    if (accessible.state == AccessibleRootState::INDEXED || accessible.state == AccessibleRootState::INVALID) {
        return accessible.state;
    }
    std::vector<unsigned char> metadata;
    bool missing{false};
    std::set<ChunkId> seen;
    const auto fetched = FetchEncryptedChunkTree(
        std::span<const unsigned char, 32>{m_runtime.GetNetworkId().begin(), 32}, accessible.content_key,
        accessible.root_chunk_id,
        [&](const ChunkId& id) {
            auto bytes = m_storage.Fetch(id);
            if (!bytes) missing = true;
            return bytes;
        },
        [&](std::span<const unsigned char> cbor) {
            metadata.assign(cbor.begin(), cbor.end());
            return true;
        },
        [&](const ChunkId& id) { return seen.insert(id).second; },
        [](std::span<const unsigned char> data) { return data.empty(); },
        MAX_ROOT_PLAINTEXT_BYTES);
    AccessibleRootState state{AccessibleRootState::INVALID};
    if (!fetched) {
        state = missing ? AccessibleRootState::TEMPORARILY_UNAVAILABLE : AccessibleRootState::INVALID;
    } else if (const auto document = DecodePrivateApplicationDocument(metadata)) {
        const bool applied = std::visit([&](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, MailMessage>) return ApplyMail(operation_id, accessible, value);
            else if constexpr (std::is_same_v<T, FilesMutationBatch>) return ApplyFiles(operation_id, accessible, value);
            else return ApplyBridge(operation_id, accessible, value);
        }, *document);
        state = applied ? AccessibleRootState::INDEXED : AccessibleRootState::INVALID;
    }
    crypto::CleanseMemory(metadata.data(), metadata.size());
    accessible.state = state;
    SaveAccessible(operation_id, accessible);
    if (state == AccessibleRootState::TEMPORARILY_UNAVAILABLE) AddId(m_application_db, UNAVAILABLE_KEY, operation_id);
    return state;
}

bool ApplicationService::ApplyMail(const uint256& operation_id, const Accessible& accessible, const MailMessage& message)
{
    const auto me = m_identity.GetAccountId();
    if (!me) return false;
    const bool outgoing = accessible.sender == *me;
    // A message opened by this Identity must be addressed to it unless it sent it.
    if (!outgoing && message.recipient_account_id != *me) return false;
    if (LoadMail(message.message_id)) return true;
    MailRecord record;
    record.operation_id = operation_id;
    record.finalized_height = accessible.height;
    record.operation_index = accessible.operation_index;
    record.sender = accessible.sender;
    record.outgoing = outgoing;
    record.message = message;
    record.folder = outgoing && message.recipient_account_id != *me ? MailFolder::SENT : MailFolder::INBOX;
    record.read = outgoing;
    return SaveMail(record);
}

bool ApplicationService::ApplyFiles(const uint256& operation_id, const Accessible& accessible,
    const FilesMutationBatch& batch)
{
    // The Files catalog is private to its owner.
    const auto me = m_identity.GetAccountId();
    if (!me || accessible.sender != *me) return false;
    for (std::uint32_t i{0}; i < batch.mutations.size(); ++i) {
        const auto& mutation = batch.mutations[i];
        const PrivateOrder order{accessible.height, accessible.operation_index, i};
        // Last canonical mutation wins, even when roots are indexed out of order.
        if (const auto existing = LoadFile(mutation.item_id); existing && existing->order >= order) continue;
        FileRecord record;
        record.operation_id = operation_id;
        record.order = order;
        if (mutation.kind == FileMutationKind::DELETE_ITEM) {
            record.deleted = true;
            record.item.item_id = mutation.item_id;
        } else {
            if (!mutation.item) return false;
            record.item = *mutation.item;
        }
        if (!SaveFile(record)) return false;
    }
    return true;
}

bool ApplicationService::ApplyBridge(const uint256& operation_id, const Accessible& accessible,
    const IdentityRecoveryBridge& bridge)
{
    const auto me = m_identity.GetAccountId();
    if (!me || accessible.sender != *me || bridge.account_id != *me) return false;
    const auto document = EncodePrivateApplicationDocument(bridge);
    if (!document) return false;
    const bool saved = m_application_db.Put(BridgeKey(operation_id), *document);
    auto wipe = *document;
    crypto::CleanseMemory(wipe.data(), wipe.size());
    return saved && AddId(m_application_db, BRIDGE_INDEX_KEY, operation_id);
}

bool ApplicationService::ImportBridgeSeeds(const AccountId& me, const std::uint64_t my_key_epoch)
{
    // Epochs whose publications have already been scanned with the recovered key.
    auto recovered = m_application_db.Get(RECOVERED_EPOCHS_KEY).value_or(std::vector<unsigned char>{});
    const auto was_recovered = [&recovered](std::uint64_t epoch) {
        for (std::size_t offset{0}; offset + 8 <= recovered.size(); offset += 8) {
            std::uint64_t value{0};
            for (unsigned i{0}; i < 8; ++i) value |= std::uint64_t{recovered[offset + i]} << (8 * i);
            if (value == epoch) return true;
        }
        return false;
    };
    bool newly_recovered{false};
    for (const auto& id : ReadIds<uint256>(m_application_db, BRIDGE_INDEX_KEY)) {
        const auto encoded = m_application_db.Get(BridgeKey(id));
        auto document = encoded ? DecodePrivateApplicationDocument(*encoded) : std::nullopt;
        if (!document || !std::holds_alternative<IdentityRecoveryBridge>(*document)) continue;
        auto& bridge = std::get<IdentityRecoveryBridge>(*document);
        for (auto& entry : bridge.historical_seeds) {
            if (entry.key_epoch < my_key_epoch && !m_identity.HasKemSeedForEpoch(entry.key_epoch, my_key_epoch)) {
                // Accept a seed only if it reproduces that epoch's canonical KEM package.
                const auto canonical = m_runtime.FindIdentityKemPackage(me, entry.key_epoch);
                const auto derived = DeriveXWingPublicKey(entry.seed);
                const auto package = derived ? EncodeIdentityKemPackage(*derived) : std::nullopt;
                if (canonical.status == IdentityKemPackageLookupStatus::FOUND && package &&
                    *package == canonical.package && m_identity.ImportHistoricalKemSeed(entry.key_epoch, entry.seed) &&
                    !was_recovered(entry.key_epoch)) {
                    Writer out;
                    out.U64(entry.key_epoch);
                    recovered.insert(recovered.end(), out.Out().begin(), out.Out().end());
                    newly_recovered = true;
                }
            }
            crypto::CleanseMemory(entry.seed.data(), entry.seed.size());
        }
    }
    if (newly_recovered) m_application_db.Put(RECOVERED_EPOCHS_KEY, recovered);
    return newly_recovered;
}

/* ---- queries and local state ---- */

std::vector<MailRecord> ApplicationService::ListMail()
{
    std::lock_guard lock{m_mutex};
    std::vector<MailRecord> records;
    for (const auto& id : ReadIds<PrivateItemId>(m_application_db, MAIL_INDEX_KEY)) {
        if (auto record = LoadMail(id)) records.push_back(std::move(*record));
    }
    std::sort(records.begin(), records.end(), [](const MailRecord& a, const MailRecord& b) {
        return std::tie(a.finalized_height, a.operation_index) > std::tie(b.finalized_height, b.operation_index);
    });
    return records;
}

std::optional<MailRecord> ApplicationService::GetMail(const PrivateItemId& id)
{
    std::lock_guard lock{m_mutex};
    return LoadMail(id);
}

bool ApplicationService::SetMailRead(const PrivateItemId& id, const bool read)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    record->read = read;
    return SaveMail(*record);
}

bool ApplicationService::SetMailStarred(const PrivateItemId& id, const bool starred)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    record->starred = starred;
    return SaveMail(*record);
}

bool ApplicationService::MoveMail(const PrivateItemId& id, const MailFolder folder)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    // Sent mail never becomes Inbox mail and vice versa.
    if ((folder == MailFolder::SENT && !record->outgoing) || (folder == MailFolder::INBOX && record->outgoing &&
            record->message.recipient_account_id != record->sender)) return false;
    record->folder = folder;
    return SaveMail(*record);
}

std::vector<FileRecord> ApplicationService::ListFiles()
{
    std::lock_guard lock{m_mutex};
    std::vector<FileRecord> records;
    for (const auto& id : ReadIds<PrivateItemId>(m_application_db, FILES_INDEX_KEY)) {
        auto record = LoadFile(id);
        if (record && !record->deleted) records.push_back(std::move(*record));
    }
    return records;
}

std::optional<FileRecord> ApplicationService::GetFile(const PrivateItemId& id)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadFile(id);
    if (!record || record->deleted) return std::nullopt;
    return record;
}

std::vector<IdentityRecoveryBridge> ApplicationService::RecoveryBridges()
{
    std::lock_guard lock{m_mutex};
    std::vector<std::pair<PrivateOrder, IdentityRecoveryBridge>> bridges;
    for (const auto& id : ReadIds<uint256>(m_application_db, BRIDGE_INDEX_KEY)) {
        const auto accessible = LoadAccessible(id);
        const auto encoded = m_application_db.Get(BridgeKey(id));
        auto document = encoded ? DecodePrivateApplicationDocument(*encoded) : std::nullopt;
        if (!accessible || !document || !std::holds_alternative<IdentityRecoveryBridge>(*document)) continue;
        bridges.emplace_back(PrivateOrder{accessible->height, accessible->operation_index, 0},
            std::move(std::get<IdentityRecoveryBridge>(*document)));
    }
    std::sort(bridges.begin(), bridges.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::vector<IdentityRecoveryBridge> out;
    for (auto& [_, bridge] : bridges) out.push_back(std::move(bridge));
    return out;
}

std::optional<AccessibleRootState> ApplicationService::PublicationState(const uint256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto accessible = LoadAccessible(operation_id);
    if (!accessible) return std::nullopt;
    return accessible->state;
}

} // namespace cybou
