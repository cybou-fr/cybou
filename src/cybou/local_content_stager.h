// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#ifndef CYBOU_LOCAL_CONTENT_STAGER_H
#define CYBOU_LOCAL_CONTENT_STAGER_H
#include <cybou/publication_service.h>
#include <cybou/chunk_retention.h>
namespace cybou {
class ChunkBlobStore;
class PrivateApplicationStore;
struct LocalPreparedContent {
    PreparedPublicationBundle bundle;
    std::vector<ChunkId> leaves;
};
/// Only local encrypted chunk I/O; no node runtime or transport.
class LocalContentStager final {
public:
    LocalContentStager(ChunkBlobStore& blobs, ChunkRetentionRegistry& retention,
        const Hash256& binding, const AccountId& account, PrivateApplicationStore* local_db = nullptr);
    using Metadata = std::function<std::optional<std::vector<unsigned char>>(std::span<const EncryptedTreeSummary>)>;
    std::optional<LocalPreparedContent> Prepare(std::string_view job, std::vector<NewContent>& children,
        const Metadata& metadata);
    bool Release(std::string_view job);
private:
    bool Journal(std::string_view job, bool add);
    RetentionKey Key(std::string_view job) const;
    ChunkBlobStore& m_blobs;
    ChunkRetentionRegistry& m_retention;
    Hash256 m_binding;
    AccountId m_account;
    PrivateApplicationStore* m_local_db;
    std::recursive_mutex m_mutex;
};
}
#endif
