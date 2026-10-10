// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/p2p/session.h>
#include <cybou/identity_crypto.h>
#include <cybou/chunk_id.h>
#include <algorithm>
#include <string_view>
namespace cybou::p2p {
namespace {
constexpr std::size_t STORAGE_ED25519_KEY{32}, STORAGE_MLDSA_KEY{1312}, STORAGE_ED25519_SIG{64}, STORAGE_MLDSA_SIG{2420};
constexpr std::size_t STORAGE_PROOF_SIZE{STORAGE_ED25519_KEY + STORAGE_MLDSA_KEY + STORAGE_ED25519_SIG + STORAGE_MLDSA_SIG};
}
std::optional<StorageId> VerifyStorageProof(const std::span<const unsigned char> payload,
    const std::span<const unsigned char> message)
{
    if (payload.size() != STORAGE_PROOF_SIZE) return std::nullopt;
    IdentityHybridPublicKey key{.purpose = IdentityKeyPurpose::STORAGE};
    std::copy_n(payload.begin(), STORAGE_ED25519_KEY, key.ed25519.begin());
    key.ml_dsa.assign(payload.begin() + STORAGE_ED25519_KEY, payload.begin() + STORAGE_ED25519_KEY + STORAGE_MLDSA_KEY);
    IdentityHybridSignature signature;
    const auto sig = payload.subspan(STORAGE_ED25519_KEY + STORAGE_MLDSA_KEY);
    std::copy_n(sig.begin(), STORAGE_ED25519_SIG, signature.ed25519.begin());
    signature.ml_dsa.assign(sig.begin() + STORAGE_ED25519_SIG, sig.end());
    if (!VerifyIdentityMessage(key, signature, message)) return std::nullopt;
    // `StorageId` коммитит оба публичных ключа под отдельным provider domain,
    // чтобы его нельзя было спутать ни с AccountID, ни с иными 32-байтовыми идентификаторами.
    constexpr std::string_view DOMAIN{"CYBOU/STORAGE-ID"};
    std::vector<unsigned char> id_input;
    id_input.reserve(DOMAIN.size() + STORAGE_ED25519_KEY + STORAGE_MLDSA_KEY);
    id_input.insert(id_input.end(), DOMAIN.begin(), DOMAIN.end());
    id_input.insert(id_input.end(), payload.begin(), payload.begin() + STORAGE_ED25519_KEY + STORAGE_MLDSA_KEY);
    return ComputeChunkId(id_input);
}

}
