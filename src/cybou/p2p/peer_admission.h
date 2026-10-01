// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_P2P_PEER_ADMISSION_H
#define CYBOU_P2P_PEER_ADMISSION_H

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace cybou::p2p {

class GeoDatabaseUpdater;

/** Integrity-checked local DB-IP country CSV reduced to French IP ranges. */
class FrenchIpDataset final {
public:
    static std::optional<std::chrono::year_month> ParseIssuedMonth(std::string_view text);
    static std::shared_ptr<const FrenchIpDataset> LoadDbIpCountryCsv(const std::filesystem::path& path,
        const std::array<unsigned char, 32>& expected_sha256, std::chrono::year_month issued_month,
        std::chrono::sys_days today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));

    bool IsFrench(std::string_view numeric_address) const;

private:
    struct Range {
        std::array<unsigned char, 16> first{};
        std::array<unsigned char, 16> last{};
        bool ipv6{false};
    };

    explicit FrenchIpDataset(std::vector<Range> ranges) : m_ranges{std::move(ranges)} {}
    std::vector<Range> m_ranges;
};

/** Local policy only; it has no consensus, Identity, or Authority effect. */
class PeerAdmissionPolicy final {
public:
    static PeerAdmissionPolicy Public(std::shared_ptr<const FrenchIpDataset> dataset);
    static PeerAdmissionPolicy PublicWithUpdater(std::shared_ptr<GeoDatabaseUpdater> updater);
    static PeerAdmissionPolicy Lab();

    bool Allows(std::string_view numeric_address) const;
    bool Ready() const;

private:
    PeerAdmissionPolicy(std::shared_ptr<const FrenchIpDataset> dataset, bool lab)
        : m_dataset{std::move(dataset)}, m_lab{lab} {}

    std::shared_ptr<const FrenchIpDataset> m_dataset;
    std::shared_ptr<GeoDatabaseUpdater> m_updater;
    bool m_lab{false};
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_PEER_ADMISSION_H
