// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// Реализация rebuildable-индекса Mail/Files и восстановления приватных публикаций.

#include <cybou/application_service.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/crypto/sha256.h>
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

constexpr std::array<unsigned char, 4> ACCESSIBLE_MAGIC{'C', 'Y', 'A', 'P'};
constexpr std::array<unsigned char, 4> MAIL_MAGIC{'C', 'Y', 'M', 'L'};
constexpr std::array<unsigned char, 4> FILE_MAGIC{'C', 'Y', 'F', 'R'};
constexpr std::string_view SCAN_KEY{"app/scan-height"};
constexpr std::string_view UNAVAILABLE_KEY{"app/unavailable"};
constexpr std::string_view MAIL_INDEX_KEY{"mail/index"};
constexpr std::string_view FILES_INDEX_KEY{"files/index"};
constexpr std::string_view BRIDGE_INDEX_KEY{"recovery/index"};
constexpr std::string_view RECOVERED_EPOCHS_KEY{"recovery/recovered-epochs"};
constexpr std::string_view OWN_PUBLICATIONS_KEY{"storage/owned-publications"};
constexpr std::string_view STORAGE_RECOVERY_INDEX_READY_KEY{"storage/recovery-index-ready"};
/** 2: однократный полный re-index ремонтирует записи, созданные до атомарных batch в Application DB. */
constexpr std::string_view DRAFT_INDEX_KEY{"mail/drafts"};
constexpr std::array<unsigned char, 4> DRAFT_MAGIC{'C', 'Y', 'D', 'R'};
constexpr std::size_t MAX_DRAFT_TEXT{1U << 20};
constexpr std::size_t MAX_DRAFT_ATTACHMENTS{64};

bool ValidDraftId(std::string_view id)
{
    return !id.empty() && id.size() <= 64 && std::all_of(id.begin(), id.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-';
    });
}

std::string DraftKey(std::string_view id) { return "mail/draft/" + std::string{id}; }
/** 0: metadata root сам не несёт application plaintext; содержимое живёт только в дочерних encrypted trees. */
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

std::string AccessibleKey(const cybou::Hash256& id) { return "app/pub/" + id.GetHex(); }
std::string MailKey(const PrivateItemId& id) { return "mail/msg/" + Hex(id); }
std::string FileKey(const PrivateItemId& id) { return "files/item/" + Hex(id); }
std::string FileStarKey(const PrivateItemId& id) { return "files/starred/" + Hex(id); }
std::string BridgeKey(const cybou::Hash256& id) { return "recovery/bridge/" + id.GetHex(); }

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

bool ReadHash256(Reader& in, cybou::Hash256& out) { return in.Bytes(std::span{out.begin(), 32}); }

/** Индексы — только фиксированные 32-байтовые ID; сама Application DB не считается перечислимым каталогом. */
template <typename Id>
std::vector<Id> ReadIds(const PrivateApplicationStore& db, std::string_view key)
{
    std::vector<Id> ids;
    const auto encoded = db.Get(key);
    if (!encoded || encoded->size() % 32 != 0) return ids;
    ids.reserve(encoded->size() / 32);
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

std::optional<ApplicationService::Accessible> ApplicationService::LoadAccessible(const cybou::Hash256& id) const
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

bool ApplicationService::SaveAccessible(const cybou::Hash256& id, const Accessible& record)
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
    if (!Magic(in, MAIL_MAGIC) || !ReadHash256(in, record.operation_id)) return std::nullopt;
    const auto height = in.U64();
    const auto index = in.U32();
    const auto sender = ReadAccount(in);
    const auto outgoing = in.U8();
    const auto folder = in.U8();
    const auto read = in.U8();
    const auto starred = in.U8();
    if (!height || !index || !sender || !outgoing || !folder || *folder < 1 ||
        *folder > static_cast<std::uint8_t>(MailFolder::DELETED) || !read || !starred) {
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
    if (!Magic(in, FILE_MAGIC) || !ReadHash256(in, record.operation_id) || !in.Bytes(stored_id) ||
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
    record.starred = m_application_db.Has(FileStarKey(id));
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
    const auto index_ready = m_application_db.Get(STORAGE_RECOVERY_INDEX_READY_KEY);
    if (!index_ready || *index_ready != std::vector<unsigned char>{1}) {
        // Backfill the own-publication index and repair records whose index
        // entries were lost before writes became atomic. Idempotent.
        Writer ready;
        ready.U8(1);
        Writer checkpoint;
        checkpoint.U64(0);
        PrivateApplicationStore::Batch batch{m_application_db};
        if (!m_application_db.Put(SCAN_KEY, checkpoint.Out()) ||
            !m_application_db.Put(STORAGE_RECOVERY_INDEX_READY_KEY, ready.Out()) || !batch.Commit()) return progress;
        height = 0;
        m_repairing = true;
    }
    // Retry roots that were unavailable earlier, with backoff (30 s doubling to 1 h): each retry
    // fetches over the network, and lost content must not be requested on every pass.
    const auto now = std::chrono::steady_clock::now();
    for (const auto& operation_id : ReadIds<cybou::Hash256>(m_application_db, UNAVAILABLE_KEY)) {
        auto& retry = m_unavailable_retry[operation_id];
        if (now < retry.next) continue;
        // The indexed records, the new state and the retry list change together.
        PrivateApplicationStore::Batch batch{m_application_db};
        auto accessible = LoadAccessible(operation_id);
        if (!accessible || Index(operation_id, *accessible) != AccessibleRootState::TEMPORARILY_UNAVAILABLE) {
            RemoveId(m_application_db, UNAVAILABLE_KEY, operation_id);
            m_unavailable_retry.erase(operation_id);
        } else {
            retry.attempts = std::min(retry.attempts + 1, 8U);
            retry.next = now + std::min(std::chrono::seconds{30} * (1U << (retry.attempts - 1)),
                std::chrono::seconds{std::chrono::hours{1}});
        }
        batch.Commit();
    }
    // The current KEM epoch comes from canonical state, never from local data.
    const auto me = m_identity.GetAccountId();
    const auto loaded = m_runtime.GetStore().GetStateSnapshot();
    const auto* identity = me && loaded && loaded.state ? loaded.state->identities.Find(*me) : nullptr;
    if (!identity) {
        progress.scanned_height = height;
        return progress;
    }
    const std::uint64_t my_key_epoch = identity->key_epoch;
    // Nothing can exist for this Identity before the block that created it:
    // no own publication and no capsule for its keys. Scanning starts there
    // (canonical account state), not at genesis.
    std::uint64_t first_height{0};
    if (const auto account = loaded.state->accounts.find(*me); account != loaded.state->accounts.end() &&
        account->second.creation_height > 0) {
        first_height = account->second.creation_height - 1;
    }
    // Historical KEM keys live only in memory; bring them back before scanning.
    if (ImportBridgeSeeds(*me, my_key_epoch)) height = 0;
    height = std::max(height, first_height);
    for (int pass{0}; pass < 2; ++pass) {
        const auto scan = m_runtime.ScanFinalizedPublications(height, progress.finalized_height, max_blocks);
        if (!scan) break;
        bool complete{true};
        for (const auto publication_height : scan->heights) {
            // Sparse public coordinates skip unrelated blocks. Private records
            // and the checkpoint past each relevant block still commit atomically.
            PrivateApplicationStore::Batch batch{m_application_db};
            if (!ProcessBlock(publication_height, my_key_epoch)) { complete = false; break; }
            Writer out;
            out.U64(publication_height);
            if (!m_application_db.Put(SCAN_KEY, out.Out()) || !batch.Commit()) { complete = false; break; }
            height = publication_height;
        }
        if (!complete) break;
        if (height < scan->scanned_height) {
            Writer out;
            out.U64(scan->scanned_height);
            if (!m_application_db.Put(SCAN_KEY, out.Out())) break;
            height = scan->scanned_height;
        }
        // A bridge found during this scan may open older publications: rescan once.
        if (height < progress.finalized_height || !ImportBridgeSeeds(*me, my_key_epoch)) break;
        height = first_height;
    }
    if (height == 0) {
        Writer out;
        out.U64(0);
        m_application_db.Put(SCAN_KEY, out.Out());
    }
    progress.scanned_height = Checkpoint();
    if (progress.scanned_height >= progress.finalized_height) m_repairing = false;
    RecoverOwnPublications(4);
    progress.unavailable_roots = static_cast<std::uint32_t>(ReadIds<cybou::Hash256>(m_application_db, UNAVAILABLE_KEY).size());
    progress.lost_roots = LostRootsLocked();
    return progress;
}

std::uint32_t ApplicationService::LostRootsLocked() const
{
    std::uint32_t lost{0};
    for (const auto& operation_id : ReadIds<cybou::Hash256>(m_application_db, UNAVAILABLE_KEY)) {
        const auto retry = m_unavailable_retry.find(operation_id);
        if (retry != m_unavailable_retry.end() && retry->second.attempts >= UNAVAILABLE_GIVE_UP_ATTEMPTS) ++lost;
    }
    return lost;
}

ApplicationScanProgress ApplicationService::Progress()
{
    std::lock_guard lock{m_mutex};
    return {.scanned_height = Checkpoint(),
        .finalized_height = m_runtime.GetFinalizedHeight().value_or(0),
        .unavailable_roots = static_cast<std::uint32_t>(ReadIds<cybou::Hash256>(m_application_db, UNAVAILABLE_KEY).size()),
        .lost_roots = LostRootsLocked()};
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
    const cybou::Hash256& operation_id, const AuthorizedRootPublication& publication, const std::uint64_t my_key_epoch)
{
    // Revoked by its author (DEC-271): its chunks are purged, so there is
    // nothing left to open; content already indexed here stays in this DB.
    if (!m_runtime.IsPublicationActive(operation_id)) return true;
    // Idempotent: a recorded publication is not reopened. One that was
    // recorded but never indexed (older databases, or an unavailable root) is
    // indexed again, and the owner index is repaired.
    if (m_application_db.Has(AccessibleKey(operation_id))) {
        auto accessible = LoadAccessible(operation_id);
        if (!accessible) return true;
        const auto me = m_identity.GetAccountId();
        if (me && accessible->sender == *me && !AddId(m_application_db, OWN_PUBLICATIONS_KEY, operation_id)) return false;
        if (accessible->state == AccessibleRootState::DISCOVERED ||
            accessible->state == AccessibleRootState::TEMPORARILY_UNAVAILABLE) {
            Index(operation_id, *accessible);
        } else if (m_repairing && accessible->state == AccessibleRootState::INDEXED) {
            accessible->state = AccessibleRootState::DISCOVERED;
            Index(operation_id, *accessible);
        }
        return true;
    }
    const auto network = std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32};
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
        // Recorded and indexed in the enclosing block batch: all or nothing.
        if (!SaveAccessible(operation_id, accessible)) return false;
        if (accessible.sender == *m_identity.GetAccountId() &&
            !AddId(m_application_db, OWN_PUBLICATIONS_KEY, operation_id)) return false;
        Index(operation_id, accessible);
        return true;
    }
    // Not for this Identity: nothing is recorded.
    return true;
}

void ApplicationService::RecoverOwnPublications(const std::uint32_t max_publications)
{
    std::uint32_t attempted{0};
    const auto me = m_identity.GetAccountId();
    if (!me) return;
    for (const auto& operation_id : ReadIds<cybou::Hash256>(m_application_db, OWN_PUBLICATIONS_KEY)) {
        const auto durability = m_storage.GetDurability(operation_id);
        if (durability && durability->state == DurabilityState::PROTECTED) {
            m_storage.Track(operation_id); // keep restored placements under audit
            continue;
        }
        if (attempted >= max_publications) break;
        auto accessible = LoadAccessible(operation_id);
        if (!accessible || accessible->sender != *me) continue;
        ++attempted;
        RecoverPlacement(operation_id, *accessible);
    }
}

bool ApplicationService::RecoverPlacement(const cybou::Hash256& operation_id, Accessible& accessible)
{
    const auto publication = m_runtime.FindFinalizedRootPublication(operation_id);
    if (!publication || publication->chunk_count == 0 || publication->chunk_count > (1U << 20)) return false;
    const auto network = std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32};
    std::vector<unsigned char> metadata;
    bool duplicate{false};
    std::set<ChunkId> candidates;
    const auto collect = [&](const ChunkId& id) {
        if (!candidates.insert(id).second) duplicate = true;
        return candidates.size() <= publication->chunk_count && !duplicate;
    };
    const auto main = FetchEncryptedChunkTree(network, accessible.content_key, accessible.root_chunk_id,
        [&](const ChunkId& id) { return m_storage.Fetch(id); },
        [&](std::span<const unsigned char> cbor) {
            metadata.assign(cbor.begin(), cbor.end());
            return true;
        },
        [](const ChunkId&) { return true; },
        [](std::span<const unsigned char> data) { return data.empty(); }, 0);
    if (!main) {
        crypto::CleanseMemory(metadata.data(), metadata.size());
        return false;
    }
    if (!EnumerateEncryptedTreeChunks(network, accessible.content_key, accessible.root_chunk_id,
            [&](const ChunkId& id) { return m_storage.Fetch(id); }, collect)) {
        crypto::CleanseMemory(metadata.data(), metadata.size());
        return false;
    }
    auto document = DecodePrivateApplicationDocument(metadata);
    crypto::CleanseMemory(metadata.data(), metadata.size());
    if (!document) return false;

    const auto process_child = [&](const ChunkId& root, ContentKey& key) {
        bool ok{true};
        // Reused protected content is not part of this publication. Its root
        // has no admission proof under this operation ID.
        if (m_storage.GetAuthorizationProof(operation_id, root)) {
            ok = EnumerateEncryptedTreeChunks(network, key, root,
                [&](const ChunkId& id) { return m_storage.Fetch(id); }, collect);
        }
        crypto::CleanseMemory(key.data(), key.size());
        return ok;
    };
    bool children_ok{true};
    if (std::holds_alternative<MailMessage>(*document)) {
        for (auto& attachment : std::get<MailMessage>(*document).attachments) {
            children_ok = process_child(attachment.root_chunk_id, attachment.content_key) && children_ok;
        }
    } else if (std::holds_alternative<FilesMutationBatch>(*document)) {
        for (auto& mutation : std::get<FilesMutationBatch>(*document).mutations) {
            if (mutation.kind == FileMutationKind::UPSERT_ITEM && mutation.item &&
                mutation.item->root_chunk_id && mutation.item->content_key) {
                children_ok = process_child(*mutation.item->root_chunk_id, *mutation.item->content_key) && children_ok;
            }
        }
    } else {
        auto& bridge = std::get<IdentityRecoveryBridge>(*document);
        for (auto& seed : bridge.historical_seeds) crypto::CleanseMemory(seed.seed.data(), seed.seed.size());
    }
    if (!children_ok) return false;
    std::vector<ChunkId> ordered_candidates(candidates.begin(), candidates.end());
    const auto recovered = m_storage.Rebuild(operation_id, ordered_candidates);
    return recovered.state == DurabilityState::PROTECTED;
}

AccessibleRootState ApplicationService::Index(const cybou::Hash256& operation_id, Accessible& accessible)
{
    if (accessible.state == AccessibleRootState::INDEXED || accessible.state == AccessibleRootState::INVALID) {
        return accessible.state;
    }
    std::vector<unsigned char> metadata;
    bool missing{false};
    std::set<ChunkId> seen;
    const auto fetched = FetchEncryptedChunkTree(
        std::span<const unsigned char, 32>{m_runtime.GetNetworkBinding().begin(), 32}, accessible.content_key,
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

bool ApplicationService::ApplyMail(const cybou::Hash256& operation_id, const Accessible& accessible, const MailMessage& message)
{
    const auto me = m_identity.GetAccountId();
    if (!me) return false;
    const bool outgoing = accessible.sender == *me;
    // A message opened by this Identity must be addressed to it unless it sent it.
    if (!outgoing && message.recipient_account_id != *me) return false;
    // Already recorded: make sure it is listed (repairs pre-batch databases).
    if (LoadMail(message.message_id)) return AddId(m_application_db, MAIL_INDEX_KEY, message.message_id);
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

bool ApplicationService::ApplyFiles(const cybou::Hash256& operation_id, const Accessible& accessible,
    const FilesMutationBatch& batch)
{
    // The Files catalog is private to its owner.
    const auto me = m_identity.GetAccountId();
    if (!me || accessible.sender != *me) return false;
    for (std::uint32_t i{0}; i < batch.mutations.size(); ++i) {
        const auto& mutation = batch.mutations[i];
        const PrivateOrder order{accessible.height, accessible.operation_index, i};
        // Last canonical mutation wins, even when roots are indexed out of order.
        if (const auto existing = LoadFile(mutation.item_id); existing && existing->order >= order) {
            if (!AddId(m_application_db, FILES_INDEX_KEY, mutation.item_id)) return false;
            continue;
        }
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

bool ApplicationService::ApplyBridge(const cybou::Hash256& operation_id, const Accessible& accessible,
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
    for (const auto& id : ReadIds<cybou::Hash256>(m_application_db, BRIDGE_INDEX_KEY)) {
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

/* ---- drafts (device-local, never published) ---- */

namespace {
void PutString(Writer& out, std::string_view text)
{
    out.U32(static_cast<std::uint32_t>(text.size()));
    out.Bytes(std::span{reinterpret_cast<const unsigned char*>(text.data()), text.size()});
}

std::optional<std::string> GetString(Reader& in, std::size_t max)
{
    const auto size = in.U32();
    if (!size || *size > max) return std::nullopt;
    std::string text(*size, '\0');
    if (!in.Bytes(std::span{reinterpret_cast<unsigned char*>(text.data()), text.size()})) return std::nullopt;
    return text;
}

std::vector<std::string> ReadNames(const PrivateApplicationStore& db, std::string_view key)
{
    std::vector<std::string> names;
    const auto encoded = db.Get(key);
    if (!encoded) return names;
    std::string current;
    for (const unsigned char c : *encoded) {
        if (c == '\n') {
            if (!current.empty()) names.push_back(current);
            current.clear();
        } else {
            current.push_back(static_cast<char>(c));
        }
    }
    return names;
}

bool WriteNames(PrivateApplicationStore& db, std::string_view key, const std::vector<std::string>& names)
{
    std::vector<unsigned char> out;
    for (const auto& name : names) {
        out.insert(out.end(), name.begin(), name.end());
        out.push_back('\n');
    }
    return db.Put(key, out);
}
} // namespace

bool ApplicationService::SaveDraft(const MailDraft& draft)
{
    std::lock_guard lock{m_mutex};
    if (!ValidDraftId(draft.draft_id) || draft.to.size() > MAX_DRAFT_TEXT || draft.subject.size() > MAX_DRAFT_TEXT ||
        draft.body.size() > MAX_DRAFT_TEXT || draft.attachments.size() > MAX_DRAFT_ATTACHMENTS) return false;
    Writer out;
    out.Bytes(DRAFT_MAGIC);
    PutString(out, draft.draft_id);
    PutString(out, draft.to);
    PutString(out, draft.subject);
    PutString(out, draft.body);
    out.U64(draft.updated_ms);
    out.U32(static_cast<std::uint32_t>(draft.attachments.size()));
    for (const auto& attachment : draft.attachments) {
        PutString(out, attachment.name);
        out.U64(attachment.logical_size);
        PutString(out, attachment.source_path);
        PutString(out, attachment.reference_id);
    }
    PrivateApplicationStore::Batch batch{m_application_db};
    if (!m_application_db.Put(DraftKey(draft.draft_id), out.Out())) return false;
    auto names = ReadNames(m_application_db, DRAFT_INDEX_KEY);
    if (std::find(names.begin(), names.end(), draft.draft_id) == names.end()) {
        names.push_back(draft.draft_id);
        if (!WriteNames(m_application_db, DRAFT_INDEX_KEY, names)) return false;
    }
    return batch.Commit();
}

std::vector<MailDraft> ApplicationService::ListDrafts()
{
    std::lock_guard lock{m_mutex};
    std::vector<MailDraft> drafts;
    for (const auto& id : ReadNames(m_application_db, DRAFT_INDEX_KEY)) {
        const auto encoded = m_application_db.Get(DraftKey(id));
        if (!encoded) continue;
        Reader in{*encoded};
        MailDraft draft;
        auto draft_id = Magic(in, DRAFT_MAGIC) ? GetString(in, 64) : std::nullopt;
        auto to = GetString(in, MAX_DRAFT_TEXT);
        auto subject = GetString(in, MAX_DRAFT_TEXT);
        auto body = GetString(in, MAX_DRAFT_TEXT);
        const auto updated = in.U64();
        const auto count = in.U32();
        if (!draft_id || *draft_id != id || !to || !subject || !body || !updated || !count ||
            *count > MAX_DRAFT_ATTACHMENTS) continue;
        bool valid{true};
        for (std::uint32_t i{0}; i < *count && valid; ++i) {
            auto name = GetString(in, 4096);
            const auto size = in.U64();
            auto path = GetString(in, 32768);
            auto reference = GetString(in, 256);
            valid = name && size && path && reference;
            if (valid) draft.attachments.push_back({std::move(*name), *size, std::move(*path), std::move(*reference)});
        }
        if (!valid || !in.Done()) continue;
        draft.draft_id = std::move(*draft_id);
        draft.to = std::move(*to);
        draft.subject = std::move(*subject);
        draft.body = std::move(*body);
        draft.updated_ms = *updated;
        drafts.push_back(std::move(draft));
    }
    std::sort(drafts.begin(), drafts.end(), [](const MailDraft& a, const MailDraft& b) { return a.updated_ms > b.updated_ms; });
    return drafts;
}

std::optional<PrivateItemId> ApplicationService::BindDraftToMessage(std::string_view draft_id,
    const PrivateItemId& proposed_id)
{
    std::lock_guard lock{m_mutex};
    if (!ValidDraftId(draft_id) || !m_application_db.Has(DraftKey(draft_id))) return std::nullopt;
    const std::string key = "mail/draft-send/" + std::string{draft_id};
    if (const auto saved = m_application_db.Get(key)) {
        if (saved->size() != proposed_id.size()) return std::nullopt;
        PrivateItemId id;
        std::copy(saved->begin(), saved->end(), id.begin());
        return id;
    }
    if (!m_application_db.Put(key, proposed_id)) return std::nullopt;
    return proposed_id;
}

bool ApplicationService::DeleteDraft(std::string_view draft_id, bool keep_send_binding)
{
    std::lock_guard lock{m_mutex};
    if (!ValidDraftId(draft_id)) return false;
    PrivateApplicationStore::Batch batch{m_application_db};
    auto names = ReadNames(m_application_db, DRAFT_INDEX_KEY);
    std::erase(names, std::string{draft_id});
    return m_application_db.Erase(DraftKey(draft_id)) &&
        (keep_send_binding || (m_application_db.Erase("mail/draft-send/" + std::string{draft_id}) &&
            m_application_db.Erase("mail/draft-send-content/" + std::string{draft_id}))) &&
        WriteNames(m_application_db, DRAFT_INDEX_KEY, names) &&
        batch.Commit();
}

bool ApplicationService::CheckDraftSendPayload(const MailDraft& draft, bool replace)
{
    std::lock_guard lock{m_mutex};
    if (!ValidDraftId(draft.draft_id) || !m_application_db.Has(DraftKey(draft.draft_id)) ||
        !m_application_db.Has("mail/draft-send/" + draft.draft_id) ||
        draft.to.size() > MAX_DRAFT_TEXT || draft.subject.size() > MAX_DRAFT_TEXT ||
        draft.body.size() > MAX_DRAFT_TEXT || draft.attachments.size() > MAX_DRAFT_ATTACHMENTS) return false;
    Writer out;
    PutString(out, draft.to);
    PutString(out, draft.subject);
    PutString(out, draft.body);
    out.U32(static_cast<std::uint32_t>(draft.attachments.size()));
    for (const auto& attachment : draft.attachments) {
        if (attachment.name.size() > 4096 || attachment.source_path.size() > 32768 ||
            attachment.reference_id.size() > 256) return false;
        PutString(out, attachment.name);
        out.U64(attachment.logical_size);
        PutString(out, attachment.source_path);
        PutString(out, attachment.reference_id);
    }
    std::array<unsigned char, 32> digest;
    if (!crypto::ComputeSha256({out.Out()}, digest.data())) return false;
    const auto key = "mail/draft-send-content/" + draft.draft_id;
    if (replace) return m_application_db.Put(key, digest);
    const auto saved = m_application_db.Get(key);
    return saved && saved->size() == digest.size() && std::equal(saved->begin(), saved->end(), digest.begin());
}

/* ---- queries and local state ---- */

std::vector<MailRecord> ApplicationService::ListMail()
{
    std::lock_guard lock{m_mutex};
    std::vector<MailRecord> records;
    for (const auto& id : ReadIds<PrivateItemId>(m_application_db, MAIL_INDEX_KEY)) {
        if (auto record = LoadMail(id); record && record->folder != MailFolder::DELETED) records.push_back(std::move(*record));
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
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(*record) && batch.Commit();
}

bool ApplicationService::SetMailStarred(const PrivateItemId& id, const bool starred)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    record->starred = starred;
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(*record) && batch.Commit();
}

bool ApplicationService::SetFileStarred(const PrivateItemId& id, const bool starred)
{
    std::lock_guard lock{m_mutex};
    const auto record = LoadFile(id);
    if (!record || record->deleted) return false;
    if (starred) return m_application_db.Put(FileStarKey(id), std::vector<unsigned char>{1});
    return !m_application_db.Has(FileStarKey(id)) || m_application_db.Erase(FileStarKey(id));
}

bool ApplicationService::MoveMail(const PrivateItemId& id, const MailFolder folder)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    // Sent mail never becomes Inbox mail and vice versa.
    if ((folder == MailFolder::SENT && !record->outgoing) || (folder == MailFolder::INBOX && record->outgoing &&
            record->message.recipient_account_id != record->sender)) return false;
    // Delete forever only from Trash; a deleted message never comes back.
    if (record->folder == MailFolder::DELETED ||
        (folder == MailFolder::DELETED && record->folder != MailFolder::TRASH)) return false;
    record->folder = folder;
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(*record) && batch.Commit();
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
    for (const auto& id : ReadIds<cybou::Hash256>(m_application_db, BRIDGE_INDEX_KEY)) {
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

std::optional<AccessibleRootState> ApplicationService::PublicationState(const cybou::Hash256& operation_id)
{
    std::lock_guard lock{m_mutex};
    const auto accessible = LoadAccessible(operation_id);
    if (!accessible) return std::nullopt;
    return accessible->state;
}

} // namespace cybou
