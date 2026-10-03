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
    /// \param text Строка месяца из CLI или имени кэша.
    /// \return Корректный `year_month`, либо `std::nullopt` при ошибке формата/диапазона.
    static std::optional<std::chrono::year_month> ParseIssuedMonth(std::string_view text);
    /// \brief Загружает CSV, проверяет SHA-256 и возраст данных, затем выделяет только французские диапазоны.
    /// \param path Путь к CSV DB-IP Lite country.
    /// \param expected_sha256 Ожидаемый digest всего файла.
    /// \param issued_month Месяц выпуска датасета.
    /// \param today Текущая дата для age-check и тестов.
    /// \return Датасет только для FR-диапазонов, либо `nullptr` при любой ошибке/устаревании.
    /// \details Проверка fail-closed запрещает пустые, поврежденные, будущие, просроченные и слишком большие файлы.
    static std::shared_ptr<const FrenchIpDataset> LoadDbIpCountryCsv(const std::filesystem::path& path,
        const std::array<unsigned char, 32>& expected_sha256, std::chrono::year_month issued_month,
        std::chrono::sys_days today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now()));

    /// \brief Проверяет, попадает ли числовой IP-адрес в разрешенный французский диапазон.
    /// \param numeric_address Числовой IPv4/IPv6 адрес без DNS-имен.
    /// \return `true`, если адрес покрыт одним из FR-диапазонов текущего датасета.
    bool IsFrench(std::string_view numeric_address) const;

private:
    struct Range {
        /// \brief Первая включительная граница диапазона в нормализованном представлении.
        std::array<unsigned char, 16> first{};
        /// \brief Последняя включительная граница диапазона.
        std::array<unsigned char, 16> last{};
        /// \brief `false` для IPv4, `true` для IPv6.
        bool ipv6{false};
    };

    explicit FrenchIpDataset(std::vector<Range> ranges) : m_ranges{std::move(ranges)} {}
    std::vector<Range> m_ranges;
};

/// \brief Локальная политика допуска, которая не влияет на консенсус, Identity и AUTH.
class PeerAdmissionPolicy final {
public:
    /// \brief Создает готовую политику из уже загруженного датасета.
    /// \param dataset Валидный FR-датасет либо `nullptr`.
    static PeerAdmissionPolicy Public(std::shared_ptr<const FrenchIpDataset> dataset);
    /// \brief Создает политику, читающую актуальный датасет у обновлятора.
    /// \param updater Фоновый обновлятор кэша DB-IP Lite.
    static PeerAdmissionPolicy PublicWithUpdater(std::shared_ptr<GeoDatabaseUpdater> updater);

    /// \brief Возвращает true, если адрес разрешен текущим локальным датасетом.
    /// \param numeric_address Числовой IPv4/IPv6 адрес.
    /// \return `false`, если датасет недоступен, адрес нечисловой или диапазон не французский.
    bool Allows(std::string_view numeric_address) const;
    /// \brief Возвращает true, когда политика уже располагает пригодным датасетом.
    /// \return `true`, если ready-кэш уже загружен напрямую или доступен через updater.
    bool Ready() const;

private:
    PeerAdmissionPolicy(std::shared_ptr<const FrenchIpDataset> dataset)
        : m_dataset{std::move(dataset)} {}

    std::shared_ptr<const FrenchIpDataset> m_dataset;
    std::shared_ptr<GeoDatabaseUpdater> m_updater;
};

} // namespace cybou::p2p

#endif // CYBOU_P2P_PEER_ADMISSION_H
