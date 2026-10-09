// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/local_content_stager.h>
#include <cybou/chunk_blob_store.h>
#include <cybou/crypto/cleanse.h>
#include <cybou/protocol_limits.h>
#include <cybou/chunk_authorization.h>
#include <cybou/private_application_store.h>
#include <set>
#include <sstream>
namespace cybou {
LocalContentStager::LocalContentStager(ChunkBlobStore& blobs, ChunkRetentionRegistry& retention,
    const Hash256& binding, const AccountId& account, PrivateApplicationStore* local_db)
    : m_blobs{blobs}, m_retention{retention}, m_binding{binding}, m_account{account}, m_local_db{local_db}
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
            task();
        }
    }};
    if (!m_local_db) return; // memory-only component fixtures
    const auto encoded = m_local_db->Get("staging/jobs");
    if (!encoded) {
        if (m_local_db->Has("staging/jobs")) throw std::runtime_error{"cannot read staging journal"};
        return;
    }
    std::istringstream input{std::string{encoded->begin(), encoded->end()}};
    std::string job;
    while (std::getline(input, job)) {
        if (job.empty()) throw std::runtime_error{"invalid staging journal"};
        // A durable immutable intent owns its pins. An interrupted preparation
        // without that intent has no owner and becomes ordinary cache again.
        if (!m_local_db->Has("outbox/job/" + job) && !Release(job))
            throw std::runtime_error{"cannot recover interrupted local staging"};
    }
}
LocalContentStager::~LocalContentStager() { Stop(); }
void LocalContentStager::Stop()
{
    m_stopping.store(true);
    m_worker.request_stop();
    m_wake.notify_all();
    if (m_worker.joinable()) m_worker.join();
}
void LocalContentStager::Post(std::function<void()> task)
{
    std::lock_guard lock{m_queue_mutex};
    if (m_stopping.load()) return;
    m_tasks.push_back(std::move(task));
    m_wake.notify_all();
}
bool LocalContentStager::Journal(std::string_view job, bool add)
{
    std::lock_guard lock{m_mutex};
    if (!m_local_db) return true;
    PrivateApplicationStore::Batch batch{*m_local_db};
    const auto encoded = m_local_db->Get("staging/jobs");
    if (!encoded && m_local_db->Has("staging/jobs")) return false;
    std::set<std::string> jobs;
    if (encoded) {
        std::istringstream input{std::string{encoded->begin(), encoded->end()}};
        std::string value;
        while (std::getline(input, value)) {
            if (value.empty()) return false;
            jobs.insert(std::move(value));
        }
    }
    if (add) jobs.insert(std::string{job});
    else jobs.erase(std::string{job});
    std::vector<unsigned char> output;
    for (const auto& value : jobs) {
        output.insert(output.end(), value.begin(), value.end());
        output.push_back('\n');
    }
    return m_local_db->Put("staging/jobs", output) && batch.Commit();
}
RetentionKey LocalContentStager::Key(std::string_view job) const
{
    const auto& id = m_account.Value();
    return {RetentionTag("CYBOU/RETENTION/IDENTITY", std::span{id.begin(), 32}),
        RetentionTag("CYBOU/RETENTION/LOCAL-OUTBOX", std::span{reinterpret_cast<const unsigned char*>(job.data()), job.size()})};
}
bool LocalContentStager::Release(std::string_view job)
{
    std::lock_guard lock{m_mutex};
    return m_retention.Release(Key(job), 0) && Journal(job, false);
}
std::optional<LocalPreparedContent> LocalContentStager::Prepare(std::string_view job,
    std::vector<NewContent>& children, const Metadata& metadata)
{
    if (m_stopping.load()) return std::nullopt;
    if (job.empty() || job.size() > 128 || std::any_of(job.begin(), job.end(), [](unsigned char c) { return c < 33 || c > 126; }))
        return std::nullopt;
    if (m_local_db && m_local_db->Has("outbox/job/" + std::string{job})) return std::nullopt;
    if (!Journal(job, true)) return std::nullopt;
    struct Rollback {
        LocalContentStager& stager;
        std::string_view job;
        bool accepted{false};
        ~Rollback() { if (!accepted) (void)stager.Release(job); }
    } rollback{*this, job};
    LocalPreparedContent result;
    std::set<ChunkId> unique;
    ChunkAuthorizationAccumulator accumulator;
    std::vector<EncryptedTreeSummary> summaries;
    struct Wipe {
        std::vector<EncryptedTreeSummary>& summaries;
        ~Wipe() { for (auto& item : summaries) crypto::CleanseMemory(item.content_key.data(), item.content_key.size()); }
    } wipe{summaries};
    const auto sink = [&](std::uint32_t, const EncryptedChunk& chunk) {
        if (m_stopping.load()) return false;
        if (result.leaves.size() >= MAX_PUBLICATION_CHUNKS || !unique.insert(chunk.id).second) return false;
        const std::array<ChunkId, 1> ids{chunk.id};
        if (!m_retention.Pin(Key(job), ids)) return false;
        const auto status = m_blobs.Put(chunk.id, chunk.stored_bytes);
        if (status != ChunkBlobPutStatus::STORED && status != ChunkBlobPutStatus::ALREADY_STORED) return false;
        result.leaves.push_back(chunk.id);
        return accumulator.Add({chunk.id});
    };
    const auto network = std::span<const unsigned char, 32>{m_binding.begin(), 32};
    for (auto& child : children) {
        const auto tree = BuildEncryptedChunkTree(network, [&](std::span<unsigned char> bytes) -> std::optional<std::size_t> {
            if (m_stopping.load()) return std::nullopt;
            return child.source(bytes);
        }, sink);
        if (!tree) return std::nullopt;
        summaries.push_back(*tree);
    }
    auto encoded = metadata(summaries);
    if (!encoded) return std::nullopt;
    const auto main = BuildEncryptedChunkTree(network,
        [](std::span<unsigned char>) -> std::optional<std::size_t> { return 0; }, sink, *encoded);
    crypto::CleanseMemory(encoded->data(), encoded->size());
    const auto authorization = accumulator.Finish();
    if (!main || !authorization || authorization->chunk_count != result.leaves.size()) return std::nullopt;
    result.bundle = {main->root_chunk_id, main->content_key, authorization->root, authorization->chunk_count};
    rollback.accepted = true;
    return result;
}
}
