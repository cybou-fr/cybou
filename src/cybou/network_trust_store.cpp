// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/network_trust_store.h>
#include <cybou/crypto/sha256.h>

#include <algorithm>
#include <fstream>

namespace cybou {

namespace {

void WriteU64LE(std::vector<unsigned char>& out, uint64_t val)
{
    for (unsigned i = 0; i < 8; ++i) {
        out.push_back(static_cast<unsigned char>((val >> (8 * i)) & 0xFF));
    }
}

std::optional<uint64_t> ReadU64LE(std::span<const unsigned char> bytes, size_t& pos)
{
    if (pos + 8 > bytes.size()) return std::nullopt;
    uint64_t val = 0;
    for (unsigned i = 0; i < 8; ++i) {
        val |= static_cast<uint64_t>(bytes[pos + i]) << (8 * i);
    }
    pos += 8;
    return val;
}

std::string GetNetworkFileName(std::span<const unsigned char> network_id)
{
    uint256 id_hash;
    crypto::Sha256 hasher;
    hasher.Write(network_id.data(), network_id.size());
    hasher.Finalize(id_hash.begin());
    return id_hash.GetHex() + ".trust";
}

} // namespace

NetworkTrustStore::NetworkTrustStore(std::filesystem::path trust_dir)
    : m_trust_dir(std::move(trust_dir))
{
    std::error_code ec;
    std::filesystem::create_directories(m_trust_dir, ec);
}

std::filesystem::path NetworkTrustStore::GetRecordPath(std::span<const unsigned char> network_id) const
{
    return m_trust_dir / GetNetworkFileName(network_id);
}

std::optional<NetworkTrustRecord> NetworkTrustStore::LoadRecord(std::span<const unsigned char> network_id) const
{
    const auto path = GetRecordPath(network_id);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return std::nullopt;

    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) return std::nullopt;

    std::vector<unsigned char> data((std::istreambuf_iterator<char>(stream)),
                                    std::istreambuf_iterator<char>());
    if (data.size() < 4 + 8 + uint256::size()) return std::nullopt;

    size_t pos{0};
    uint32_t id_len = static_cast<uint32_t>(data[pos]) |
                      (static_cast<uint32_t>(data[pos + 1]) << 8) |
                      (static_cast<uint32_t>(data[pos + 2]) << 16) |
                      (static_cast<uint32_t>(data[pos + 3]) << 24);
    pos += 4;
    if (pos + id_len + 8 + uint256::size() != data.size()) return std::nullopt;

    std::vector<unsigned char> read_id(data.begin() + pos, data.begin() + pos + id_len);
    pos += id_len;

    auto gen = ReadU64LE(data, pos);
    if (!gen) return std::nullopt;

    uint256 digest;
    std::copy_n(data.begin() + pos, uint256::size(), digest.begin());

    return NetworkTrustRecord{
        .network_id = std::move(read_id),
        .highest_generation = *gen,
        .accepted_genesis_digest = digest,
    };
}

NetworkTrustDecision NetworkTrustStore::Evaluate(
    std::span<const unsigned char> expected_network_id,
    const NetworkGenesis& candidate) const
{
    // 1. Verify that candidate's network public key matches expected NetworkID
    const auto candidate_net_id = CanonicalSerializeNetworkPublicKey(candidate.network_public_key);
    if (candidate_net_id.size() != expected_network_id.size() ||
        !std::equal(candidate_net_id.begin(), candidate_net_id.end(), expected_network_id.begin())) {
        return NetworkTrustDecision::REJECT_INVALID_KEY;
    }

    // 2. Cryptographic signature check
    if (VerifySignedNetworkGenesis(candidate) != NetworkGenesisError::NONE) {
        return NetworkTrustDecision::REJECT_INVALID_SIG;
    }

    // 3. Compare with persistent anti-rollback trust record
    const auto candidate_digest = ComputeNetworkGenesisDigest(candidate);
    const auto existing = LoadRecord(expected_network_id);

    if (!existing) {
        return NetworkTrustDecision::ACCEPT_NEW;
    }

    if (candidate.genesis_generation < existing->highest_generation) {
        return NetworkTrustDecision::REJECT_ROLLBACK;
    }

    if (candidate.genesis_generation == existing->highest_generation) {
        if (candidate_digest == existing->accepted_genesis_digest) {
            return NetworkTrustDecision::ACCEPT_CURRENT;
        }
        return NetworkTrustDecision::REJECT_CONFLICT;
    }

    return NetworkTrustDecision::ACCEPT_NEW;
}

bool NetworkTrustStore::RecordAccepted(
    std::span<const unsigned char> expected_network_id,
    const VerifiedNetworkGenesis& verified_genesis)
{
    const auto decision = Evaluate(expected_network_id, verified_genesis.GetGenesis());
    if (decision != NetworkTrustDecision::ACCEPT_NEW && decision != NetworkTrustDecision::ACCEPT_CURRENT) {
        return false;
    }

    const auto digest = ComputeNetworkGenesisDigest(verified_genesis.GetGenesis());
    std::vector<unsigned char> data;
    data.reserve(4 + expected_network_id.size() + 8 + uint256::size());

    const uint32_t id_len = static_cast<uint32_t>(expected_network_id.size());
    data.push_back(static_cast<unsigned char>(id_len & 0xFF));
    data.push_back(static_cast<unsigned char>((id_len >> 8) & 0xFF));
    data.push_back(static_cast<unsigned char>((id_len >> 16) & 0xFF));
    data.push_back(static_cast<unsigned char>((id_len >> 24) & 0xFF));

    data.insert(data.end(), expected_network_id.begin(), expected_network_id.end());
    WriteU64LE(data, verified_genesis.GetGeneration());
    data.insert(data.end(), digest.begin(), digest.end());

    const auto path = GetRecordPath(expected_network_id);
    const auto tmp_path = path.string() + ".tmp";

    std::ofstream stream(tmp_path, std::ios::binary | std::ios::trunc);
    if (!stream.is_open()) return false;
    stream.write(reinterpret_cast<const char*>(data.data()), data.size());
    stream.flush();
    if (!stream.good()) {
        std::error_code ec;
        std::filesystem::remove(tmp_path, ec);
        return false;
    }
    stream.close();

    std::error_code ec;
    std::filesystem::rename(tmp_path, path, ec);
    return !ec;
}

} // namespace cybou
