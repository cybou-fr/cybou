// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_NODE_BINDING_H
#define CYBOU_NODE_BINDING_H
#include <cybou/identity_registry.h>
namespace cybou {
/** Service identity only. Bind/revoke consume the owner's ordinary authorization nonce. */
struct NodeBindingPayload {
    IdentityHybridPublicKey key{IdentityKeyPurpose::VALIDATION_NODE, {}, {}};
    bool revoke{false};
    /** Revocations identify the bound node directly and need no service key. */
    uint256 revoke_node_id;
    std::optional<IdentityHybridPublicKey> provider_key;
    friend bool operator==(const NodeBindingPayload&, const NodeBindingPayload&) = default;
};
struct AuthorizedNodeBinding {
    IdentityOperationAuthorization authorization;
    NodeBindingPayload binding;
    IdentityHybridSignature node_proof;
    std::optional<IdentityHybridSignature> provider_proof;
    friend bool operator==(const AuthorizedNodeBinding&, const AuthorizedNodeBinding&) = default;
};
struct BoundNode {
    AccountId account;
    IdentityHybridPublicKey key;
    uint64_t owner_key_epoch{0};
    std::optional<IdentityHybridPublicKey> provider_key;
    friend bool operator==(const BoundNode&, const BoundNode&) = default;
};
std::optional<std::vector<unsigned char>> SerializeNodeBindingPayload(const NodeBindingPayload& payload);
std::optional<NodeBindingPayload> DeserializeNodeBindingPayload(std::span<const unsigned char> bytes);
std::optional<IdentityKeyId> ComputeNodeBindingCommitment(const NodeBindingPayload& payload);
std::optional<std::array<unsigned char, 32>> StorageProviderId(const IdentityHybridPublicKey& key);
} // namespace cybou
#endif
