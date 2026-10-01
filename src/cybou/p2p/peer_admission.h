// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_PEER_ADMISSION_H
#define CYBOU_P2P_PEER_ADMISSION_H

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou::p2p {

/** Integrity-checked local list of French public address prefixes. */
class FrenchIpDataset final {
public:
    static std::shared_ptr<const FrenchIpDataset> Load(const std::filesystem::path& path,
        const std::array<unsigned char, 32>& expected_sha256);

    bool IsFrench(std::string_view numeric_address) const;

private:
    struct Prefix {
        std::array<unsigned char, 16> address{};
        uint8_t bits{0};
        bool ipv6{false};
    };

    explicit FrenchIpDataset(std::vector<Prefix> prefixes) : m_prefixes{std::move(prefixes)} {}
    std::vector<Prefix> m_prefixes;
};

/** Local policy only; it has no consensus, Identity, or Authority effect. */
class PeerAdmissionPolicy final {
public:
    static PeerAdmissionPolicy Public(std::shared_ptr<const FrenchIpDataset> dataset);
    static PeerAdmissionPolicy Lab();

    bool Allows(std::string_view numeric_address) const;
    bool Ready() const { return m_lab || static_cast<bool>(m_dataset); }

private:
    PeerAdmissionPolicy(std::shared_ptr<const FrenchIpDataset> dataset, bool lab)
        : m_dataset{std::move(dataset)}, m_lab{lab} {}

    std::shared_ptr<const FrenchIpDataset> m_dataset;
    bool m_lab{false};
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_PEER_ADMISSION_H
