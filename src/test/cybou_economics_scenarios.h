// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
// Developer cost model; never used for actual billing or staging.
#ifndef CYBOU_ECONOMICS_SCENARIOS_H
#define CYBOU_ECONOMICS_SCENARIOS_H
#include <cybou/encrypted_chunk_tree.h>
#include <cybou/root_publication.h>
#include <utility>
namespace cybou::test {
constexpr std::uint64_t CeilDiv(std::uint64_t n, std::uint64_t d) { return n / d + (n % d != 0); }
constexpr std::uint64_t TreeChunks(std::uint64_t data)
{
    auto total = data + 1; // ROOT, even for empty content
    while (data >= ENCRYPTED_TREE_MAX_CHILDREN) {
        data = CeilDiv(data, ENCRYPTED_TREE_MAX_CHILDREN);
        total += data;
    }
    return total;
}
constexpr std::pair<std::uint32_t, std::uint32_t> PublicationChunkBounds(std::uint64_t file_bytes)
{
    if (!file_bytes) return {1, 1}; // small private metadata-only publication
    return {static_cast<std::uint32_t>(1 + TreeChunks(CeilDiv(file_bytes, ENCRYPTED_TREE_DATA_MAX_BYTES))),
        static_cast<std::uint32_t>(1 + TreeChunks(CeilDiv(file_bytes, ENCRYPTED_TREE_DATA_MIN_BYTES)))};
}
inline AuthorizedRootPublication SamplePublication(std::uint32_t chunks, std::uint32_t periods, std::size_t capsules)
{
    RootPublication root;
    root.root_chunk_id.fill(11);
    root.chunk_authorization_root.fill(22);
    root.chunk_count = chunks;
    root.lease_periods = periods;
    root.recipient_capsules.resize(capsules);
    std::array<unsigned char, 32> account{};
    account[0] = 1;
    AuthorizedRootPublication sample{{.account_id = *AccountId::FromBytes(account), .kind = IdentityOperationKind::ROOT_PUBLICATION,
        .payload_commitment = *ComputeRootPublicationPayloadCommitment(root)}, root};
    // Wire-size placeholders only. This sample cannot authorize any operation.
    sample.authorization.signature.ed25519.fill(1);
    sample.authorization.signature.ml_dsa.assign(2420, 1);
    return sample;
}
}
#endif
