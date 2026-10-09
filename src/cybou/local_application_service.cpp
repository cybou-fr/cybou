// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/local_application_service.h>
#include <cybou/crypto/sha256.h>
#include <cybou/protocol_limits.h>
#include <algorithm>
#include <array>
#include <tuple>
#include <variant>
namespace cybou {
LocalApplicationService::LocalApplicationService(PrivateApplicationStore& db) : m_application_db{db}
{
    m_worker = std::jthread{[this](std::stop_token stop) {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock{m_queue_mutex};
                m_wake.wait(lock, stop, [this] { return !m_tasks.empty(); });
                if (m_tasks.empty() && stop.stop_requested()) break;
                if (m_tasks.empty()) continue;
                task = std::move(m_tasks.front());
                m_tasks.pop_front();
            }
            ++m_revision;
            task();
            ++m_revision;
        }
    }};
}
LocalApplicationService::~LocalApplicationService() { Stop(); }
void LocalApplicationService::Stop()
{
    m_worker.request_stop();
    m_wake.notify_all();
    if (m_worker.joinable()) m_worker.join();
}
void LocalApplicationService::Post(std::function<void()> task)
{
    std::lock_guard lock{m_queue_mutex};
    m_tasks.push_back(std::move(task));
    m_wake.notify_all();
}
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

std::string MailKey(const PrivateItemId& id) { return "mail/msg/" + Hex(id); }
std::string FileKey(const PrivateItemId& id) { return "files/item/" + Hex(id); }
std::string FileStarKey(const PrivateItemId& id) { return "files/starred/" + Hex(id); }

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

std::optional<MailRecord> LocalApplicationService::LoadMail(const PrivateItemId& id) const
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

bool LocalApplicationService::SaveMail(const MailRecord& record)
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

std::optional<FileRecord> LocalApplicationService::LoadFile(const PrivateItemId& id, bool desired) const
{
    auto encoded = desired ? m_application_db.Get("files/desired/" + Hex(id)) : std::nullopt;
    if (!encoded) encoded = m_application_db.Get(FileKey(id));
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

bool LocalApplicationService::SaveFile(const FileRecord& record, bool desired)
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
    return m_application_db.Put(desired ? "files/desired/" + Hex(record.item.item_id) : FileKey(record.item.item_id), out.Out()) &&
        AddId(m_application_db, FILES_INDEX_KEY, record.item.item_id);
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

bool LocalApplicationService::SaveDraft(const MailDraft& draft)
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
    if (!batch.Commit()) return false;
    m_revision.fetch_add(2);
    return true;
}

std::vector<MailDraft> LocalApplicationService::ListDrafts()
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

std::optional<PrivateItemId> LocalApplicationService::BindDraftToMessage(std::string_view draft_id,
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

bool LocalApplicationService::DeleteDraft(std::string_view draft_id, bool keep_send_binding)
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

bool LocalApplicationService::DeleteAcceptedDraft(std::string_view id)
{
    std::lock_guard lock{m_mutex};
    const auto drafts = ListDrafts();
    const auto current = std::find_if(drafts.begin(), drafts.end(), [&](const auto& draft) { return draft.draft_id == id; });
    if (current == drafts.end() || !CheckDraftSendPayload(*current, false) || !DeleteDraft(id, true)) return false;
    m_revision.fetch_add(2);
    return true;
}

bool LocalApplicationService::CheckDraftSendPayload(const MailDraft& draft, bool replace)
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

std::vector<MailRecord> LocalApplicationService::ListMail()
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

std::optional<MailRecord> LocalApplicationService::GetMail(const PrivateItemId& id)
{
    std::lock_guard lock{m_mutex};
    return LoadMail(id);
}

bool LocalApplicationService::SetMailRead(const PrivateItemId& id, const bool read)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    record->read = read;
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(*record) && batch.Commit();
}

bool LocalApplicationService::SetMailStarred(const PrivateItemId& id, const bool starred)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadMail(id);
    if (!record) return false;
    record->starred = starred;
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(*record) && batch.Commit();
}

bool LocalApplicationService::SetFileStarred(const PrivateItemId& id, const bool starred)
{
    std::lock_guard lock{m_mutex};
    const auto record = LoadFile(id);
    if (!record || record->deleted) return false;
    if (starred) return m_application_db.Put(FileStarKey(id), std::vector<unsigned char>{1});
    return !m_application_db.Has(FileStarKey(id)) || m_application_db.Erase(FileStarKey(id));
}

bool LocalApplicationService::MoveMail(const PrivateItemId& id, const MailFolder folder)
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

std::vector<FileRecord> LocalApplicationService::ListFiles()
{
    std::lock_guard lock{m_mutex};
    std::vector<FileRecord> records;
    for (const auto& id : ReadIds<PrivateItemId>(m_application_db, FILES_INDEX_KEY)) {
        auto record = LoadFile(id);
        if (record && !record->deleted) records.push_back(std::move(*record));
    }
    return records;
}

std::optional<FileRecord> LocalApplicationService::GetFile(const PrivateItemId& id)
{
    std::lock_guard lock{m_mutex};
    auto record = LoadFile(id);
    if (!record || record->deleted) return std::nullopt;
    return record;
}


bool LocalApplicationService::ImportMail(const MailRecord& incoming)
{
    std::lock_guard lock{m_mutex};
    auto record = incoming;
    if (const auto old = LoadMail(record.message.message_id)) {
        record.folder = old->folder;
        record.read = old->read;
        record.starred = old->starred;
        if (old->finalized_height > record.finalized_height) return true;
        if (old->operation_id == record.operation_id && old->finalized_height == record.finalized_height &&
            old->operation_index == record.operation_index && old->message == record.message) return true;
    }
    PrivateApplicationStore::Batch batch{m_application_db};
    return SaveMail(record) && batch.Commit();
}

bool LocalApplicationService::ImportFile(const FileRecord& incoming)
{
    std::lock_guard lock{m_mutex};
    if (const auto old = LoadFile(incoming.item.item_id, false); old && old->order >= incoming.order) return true;
    PrivateApplicationStore::Batch batch{m_application_db};
    if (!SaveFile(incoming)) return false;
    const auto pending = m_application_db.Get("files/desired-job/" + Hex(incoming.item.item_id));
    if (pending) {
        const std::string job_id{pending->begin(), pending->end()};
        for (const auto& job : Outbox()) {
            if (job.job_id == job_id && job.status.operation_id == incoming.operation_id && !incoming.operation_id.IsNull()) {
                if (!m_application_db.Erase("files/desired/" + Hex(incoming.item.item_id)) ||
                    !m_application_db.Erase("files/desired-job/" + Hex(incoming.item.item_id))) return false;
                break;
            }
        }
    }
    return batch.Commit();
}

bool LocalApplicationService::MigrateFrom(PrivateApplicationStore& source)
{
    std::lock_guard lock{m_mutex};
    if (m_application_db.Has("local/import-complete")) return true;
    if (!source.IsUnlocked() || source.Account() != m_application_db.Account()) return false;
    // Destination and completion marker commit together. The source remains
    // untouched, so a crash before commit safely retries the complete transfer.
    PrivateApplicationStore::Batch batch{m_application_db};
    const auto copy = [&](const std::string& name, bool required = false) {
        const auto value = source.Get(name);
        return value ? m_application_db.Put(name, *value) : !required && !source.Has(name);
    };
    for (const auto key : {MAIL_INDEX_KEY, FILES_INDEX_KEY}) {
        const auto index = source.Get(key);
        if (index && index->size() % 32 != 0) return false;
        if (!copy(std::string{key})) return false;
        for (const auto& id : ReadIds<PrivateItemId>(source, key)) {
            if (key == MAIL_INDEX_KEY) { if (!copy(MailKey(id), true)) return false; }
            else if (!copy(FileKey(id), true) || !copy(FileStarKey(id))) return false;
        }
    }
    if (!copy(std::string{DRAFT_INDEX_KEY})) return false;
    for (const auto& id : ReadNames(source, DRAFT_INDEX_KEY)) {
        if (!copy(DraftKey(id), true) || !copy("mail/draft-send/" + id) ||
            !copy("mail/draft-send-content/" + id)) return false;
    }
    // Keep durable job evidence even before outgoing jobs move to local Outbox.
    if (!copy("publication/jobs")) return false;
    for (const auto& id : ReadNames(source, "publication/jobs")) {
        for (const auto prefix : {"publication/job/", "publication/intent/", "publication/leaves/",
                                 "publication/leasing/", "publication/cancel/"})
            if (!copy(std::string{prefix} + id, std::string_view{prefix} == "publication/job/")) return false;
    }
    return m_application_db.Put("local/import-complete", std::vector<unsigned char>{1}) && batch.Commit();
}

bool LocalApplicationService::QueuePublication(std::string_view job, const LocalPreparedContent& content,
    const PrivateApplicationDocument& document, std::optional<AccountId> recipient, std::string_view draft_id)
{
    std::lock_guard lock{m_mutex};
    if (!ValidDraftId(job) || content.leaves.empty() || content.leaves.size() != content.bundle.chunk_count) return false;
    const auto encoded_document = EncodePrivateApplicationDocument(document);
    if (!encoded_document) return false;
    // Job ownership is immutable: retries reuse this exact prepared content.
    if (m_application_db.Has("outbox/job/" + std::string{job})) {
        for (const auto& existing : Outbox()) if (existing.job_id == job)
            return existing.document == document && existing.recipient == recipient &&
                existing.content.leaves == content.leaves &&
                existing.content.bundle.root_chunk_id == content.bundle.root_chunk_id &&
                existing.content.bundle.content_key == content.bundle.content_key &&
                existing.content.bundle.chunk_authorization_root == content.bundle.chunk_authorization_root &&
                existing.content.bundle.chunk_count == content.bundle.chunk_count;
        return false;
    }
    PrivateApplicationStore::Batch batch{m_application_db};
    std::uint64_t sequence{0};
    if (const auto saved = m_application_db.Get("outbox/sequence")) {
        Reader in{*saved};
        const auto n = in.U64();
        if (!n || !in.Done() || *n == std::numeric_limits<std::uint64_t>::max()) return false;
        sequence = *n;
    }
    Writer out;
    out.U64(++sequence);
    out.Bytes(content.bundle.root_chunk_id);
    out.Bytes(content.bundle.content_key);
    out.Bytes(content.bundle.chunk_authorization_root);
    out.U32(content.bundle.chunk_count);
    out.U8(recipient ? 1 : 0);
    if (recipient) out.Bytes(recipient->Value());
    for (const auto& leaf : content.leaves) out.Bytes(leaf);
    out.Bytes(*encoded_document);
    Writer counter;
    counter.U64(sequence);
    auto jobs = ReadNames(m_application_db, "outbox/index");
    jobs.emplace_back(job);
    if (!m_application_db.Put("outbox/job/" + std::string{job}, out.Out()) ||
        !m_application_db.Put("outbox/sequence", counter.Out()) || !WriteNames(m_application_db, "outbox/index", jobs)) return false;
    if (const auto* mail = std::get_if<MailMessage>(&document)) {
        MailRecord record;
        record.message = *mail;
        record.sender = m_application_db.Account();
        record.outgoing = true;
        record.read = true;
        record.folder = mail->recipient_account_id == record.sender ? MailFolder::INBOX : MailFolder::SENT;
        if (!SaveMail(record)) return false;
        if (!draft_id.empty()) {
            const auto drafts = ListDrafts();
            const auto current = std::find_if(drafts.begin(), drafts.end(), [&](const auto& draft) { return draft.draft_id == draft_id; });
            // Editing remains available during staging. Only remove the exact
            // draft payload accepted for this send, preserving subsequent edits.
            if (current != drafts.end() && CheckDraftSendPayload(*current, false) && !DeleteDraft(draft_id, true)) return false;
        }
    } else if (const auto* files = std::get_if<FilesMutationBatch>(&document)) {
        for (const auto& mutation : files->mutations) {
            FileRecord record;
            record.deleted = mutation.kind == FileMutationKind::DELETE_ITEM;
            record.item.item_id = mutation.item_id;
            if (mutation.item) record.item = *mutation.item;
            if (!SaveFile(record, true) || !m_application_db.Put("files/desired-job/" + Hex(mutation.item_id),
                std::span{reinterpret_cast<const unsigned char*>(job.data()), job.size()})) return false;
        }
    }
    if (!batch.Commit()) return false;
    // Content preparation can commit from its own executor. Keep the odd/even
    // local-command marker unchanged while invalidating older GUI snapshots.
    m_revision.fetch_add(2);
    return true;
}

std::vector<PendingPublication> LocalApplicationService::Outbox()
{
    std::lock_guard lock{m_mutex};
    std::vector<PendingPublication> result;
    for (const auto& id : ReadNames(m_application_db, "outbox/index")) {
        const auto encoded = m_application_db.Get("outbox/job/" + id);
        if (!encoded) throw std::runtime_error{"Outbox entry missing; refusing to forget local work"};
        Reader in{*encoded};
        PendingPublication job;
        job.job_id = id;
        const auto sequence = in.U64();
        if (!sequence || !in.Bytes(job.content.bundle.root_chunk_id) || !in.Bytes(job.content.bundle.content_key) ||
            !in.Bytes(job.content.bundle.chunk_authorization_root)) throw std::runtime_error{"Outbox is corrupt"};
        const auto count = in.U32();
        const auto has_recipient = in.U8();
        if (!count || *count == 0 || *count > MAX_PUBLICATION_CHUNKS || !has_recipient || *has_recipient > 1)
            throw std::runtime_error{"Outbox bounds invalid"};
        job.sequence = *sequence;
        job.content.bundle.chunk_count = *count;
        if (*has_recipient) {
            job.recipient = ReadAccount(in);
            if (!job.recipient) throw std::runtime_error{"Outbox recipient invalid"};
        }
        job.content.leaves.resize(*count);
        for (auto& leaf : job.content.leaves) if (!in.Bytes(leaf)) throw std::runtime_error{"Outbox leaves invalid"};
        const auto document = DecodePrivateApplicationDocument(in.Rest());
        if (!document) throw std::runtime_error{"Outbox document invalid"};
        job.document = *document;
        if (const auto saved = m_application_db.Get("outbox/status/" + id)) {
            Reader state{*saved};
            const auto phase = state.U8();
            if (!phase || *phase < 1 || *phase > static_cast<std::uint8_t>(PublicationJobPhase::QUEUED) ||
                !ReadHash256(state, job.status.operation_id)) throw std::runtime_error{"Outbox status invalid"};
            const auto height = state.U64();
            const auto error = GetString(state, 4096);
            if (!height || !error || !state.Done()) throw std::runtime_error{"Outbox status invalid"};
            job.status.phase = static_cast<PublicationJobPhase>(*phase);
            job.status.finalized_height = *height;
            job.status.error = *error;
        }
        result.push_back(std::move(job));
    }
    return result;
}

bool LocalApplicationService::SetPublicationStatus(std::string_view job, const PublicationJobResult& status)
{
    std::lock_guard lock{m_mutex};
    if (!m_application_db.Has("outbox/job/" + std::string{job}) || status.error.size() > 4096) return false;
    Writer out;
    out.U8(static_cast<std::uint8_t>(status.phase));
    out.Bytes(std::span{status.operation_id.begin(), 32});
    out.U64(status.finalized_height);
    PutString(out, status.error);
    return m_application_db.Put("outbox/status/" + std::string{job}, out.Out());
}

} // namespace cybou
