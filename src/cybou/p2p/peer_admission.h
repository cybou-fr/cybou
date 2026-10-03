// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API локальной политики географического допуска пиров.

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

/// \brief Локальный набор французских IP-диапазонов, загруженный из проверенного DB-IP CSV.
class FrenchIpDataset final {
public:
    /// \brief Разбирает месяц выпуска в формате `YYYY-MM`.
    static std::optional<std::chrono::year_month> ParseIssuedMonth(std::string_view text);
    /// \brief Загружает CSV, проверяет SHA-256 и возраст данных, затем выделяет только французские диапазоны.
    static std::shared_ptr<const FrenchIpDataset> LoadDbIpCountryCsv(const std::filesystem::path& path,
        const std::array<unsigned char, 32>& expected_sha256, std::chrono::year_month issued_month,
        std::chrono::sys_days today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));

    /// \brief Проверяет, попадает ли числовой IP-адрес в разрешенный французский диапазон.
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

/// \brief Локальная политика допуска, которая не влияет на консенсус, Identity и AUTH.
class PeerAdmissionPolicy final {
public:
    /// \brief Создает готовую политику из уже загруженного датасета.
    static PeerAdmissionPolicy Public(std::shared_ptr<const FrenchIpDataset> dataset);
    /// \brief Создает политику, читающую актуальный датасет у обновлятора.
    static PeerAdmissionPolicy PublicWithUpdater(std::shared_ptr<GeoDatabaseUpdater> updater);

    /// \brief Возвращает true, если адрес разрешен текущим локальным датасетом.
    bool Allows(std::string_view numeric_address) const;
    /// \brief Возвращает true, когда политика уже располагает пригодным датасетом.
    bool Ready() const;

private:
    PeerAdmissionPolicy(std::shared_ptr<const FrenchIpDataset> dataset)
        : m_dataset{std::move(dataset)} {}

    std::shared_ptr<const FrenchIpDataset> m_dataset;
    std::shared_ptr<GeoDatabaseUpdater> m_updater;
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_PEER_ADMISSION_H
