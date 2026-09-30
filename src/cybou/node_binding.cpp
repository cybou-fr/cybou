// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#include <cybou/node_binding.h>
#include <cybou/validation_service.h>
#include <cybou/crypto/sha256.h>
#include <algorithm>
namespace cybou {
std::optional<std::array<unsigned char, 32>> StorageProviderId(const IdentityHybridPublicKey& key) {
    if (key.purpose != IdentityKeyPurpose::STORAGE_PROVIDER || key.ml_dsa.size() != 1312 ||
        std::all_of(key.ed25519.begin(),key.ed25519.end(),[](auto b){return b==0;}) ||
        std::all_of(key.ml_dsa.begin(),key.ml_dsa.end(),[](auto b){return b==0;})) return std::nullopt;
    constexpr std::string_view domain{"CYBOU/PROVIDER-ID/v1"};
    std::vector<unsigned char> bytes(domain.begin(),domain.end());
    bytes.insert(bytes.end(),key.ed25519.begin(),key.ed25519.end()); bytes.insert(bytes.end(),key.ml_dsa.begin(),key.ml_dsa.end()); return ComputeBlake3Digest(bytes);
}
std::optional<std::vector<unsigned char>> SerializeNodeBindingPayload(const NodeBindingPayload& payload) {
    if (!ValidationNodeId(payload.key)) return std::nullopt;
    std::vector<unsigned char> out{1, static_cast<unsigned char>(payload.revoke)};
    out.insert(out.end(), payload.key.ed25519.begin(), payload.key.ed25519.end());
    out.insert(out.end(), payload.key.ml_dsa.begin(), payload.key.ml_dsa.end());
    out.push_back(payload.provider_key ? 1 : 0);
    if(payload.provider_key) {
        if(!StorageProviderId(*payload.provider_key))return std::nullopt;
        out.insert(out.end(),payload.provider_key->ed25519.begin(),payload.provider_key->ed25519.end());
        out.insert(out.end(),payload.provider_key->ml_dsa.begin(),payload.provider_key->ml_dsa.end());
    }
    return out;
}
std::optional<NodeBindingPayload> DeserializeNodeBindingPayload(std::span<const unsigned char> bytes) {
    if (bytes.size() < 1347 || bytes[0] != 1 || bytes[1] > 1 || bytes[1346] > 1 || bytes.size() != 1347 + (bytes[1346] ? 1344 : 0)) return std::nullopt;
    NodeBindingPayload payload; payload.revoke = bytes[1] == 1;
    std::copy_n(bytes.begin() + 2, 32, payload.key.ed25519.begin());
    payload.key.ml_dsa.assign(bytes.begin() + 34, bytes.begin() + 1346);
    if(bytes[1346]) {
        payload.provider_key.emplace(IdentityHybridPublicKey{IdentityKeyPurpose::STORAGE_PROVIDER,{}, {}});
        std::copy_n(bytes.begin()+1347,32,payload.provider_key->ed25519.begin());
        payload.provider_key->ml_dsa.assign(bytes.begin()+1379,bytes.end());
    }
    return SerializeNodeBindingPayload(payload) ? std::optional{payload} : std::nullopt;
}
std::optional<IdentityKeyId> ComputeNodeBindingCommitment(const NodeBindingPayload& payload) {
    const auto bytes = SerializeNodeBindingPayload(payload); if (!bytes) return std::nullopt;
    IdentityKeyId out{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes("CYBOU/NODE-BINDING/V1"), *bytes}, out.data())) return std::nullopt;
    return out;
}
} // namespace cybou
