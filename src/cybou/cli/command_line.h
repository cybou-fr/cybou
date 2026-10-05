// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
/// \file
/// \brief Русские вспомогательные утилиты для строгого разбора CLI-аргументов.
#ifndef CYBOU_CLI_COMMAND_LINE_H
#define CYBOU_CLI_COMMAND_LINE_H
#include <array>
#include <charconv>
#include <cstdint>
#include <initializer_list>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cybou::cli {

/// \brief Проверяет, является ли токен именем опции формата `--name`.
/// \param token Один токен argv.
/// \return `true`, если токен начинается с `--` и содержит непустое имя.
inline bool IsOptionToken(const std::string_view token) noexcept
{
    return token.starts_with("--") && token.size() > 2;
}

/// \brief Хранит разобранные пары `--option value` без позиционных аргументов.
class Options {
    std::map<std::string, std::string> m_values;
public:
    /// \brief Разбирает хвост argv, начиная с указанного индекса.
    /// \param argc Стандартное количество аргументов `main`.
    /// \param argv Стандартный массив аргументов `main`.
    /// \param start Индекс первого токена, который должен быть разобран как `--option value`.
    /// \throw std::invalid_argument При непарной записи, повторе опции или позиционном токене.
    explicit Options(int argc, char* argv[], int start) {
        for (int i = start; i < argc; ++i) {
            const std::string_view key = argv[i];
            if (!IsOptionToken(key) || i + 1 >= argc || IsOptionToken(argv[i + 1])) {
                throw std::invalid_argument("expected --option value");
            }
            if (!m_values.emplace(std::string{key.substr(2)}, std::string{argv[++i]}).second) {
                throw std::invalid_argument("duplicate option");
            }
        }
    }
    /// \brief Возвращает true, если опция присутствует.
    /// \param key Имя без префикса `--`.
    bool Has(const std::string& key) const { return m_values.contains(key); }
    /// \brief Возвращает значение опции или запасное значение.
    /// \param key Имя без префикса `--`.
    /// \param fallback Значение по умолчанию, если опция отсутствует.
    std::string Get(const std::string& key, const std::string& fallback = {}) const {
        auto it = m_values.find(key); return it == m_values.end() ? fallback : it->second;
    }
    /// \brief Возвращает обязательную опцию или выбрасывает исключение.
    /// \param key Имя без префикса `--`.
    /// \throw std::invalid_argument Если опция отсутствует или имеет пустое значение.
    std::string Require(const std::string& key) const {
        auto value = Get(key); if (value.empty()) throw std::invalid_argument("missing --" + key); return value;
    }
    /// \brief Разрешает только перечисленные опции.
    /// \param keys Белый список допустимых имен без `--`.
    void Allow(std::initializer_list<std::string> keys) const { AllowSet(std::set<std::string>(keys)); }
    /// \brief Разрешает только заранее подготовленное множество опций.
    /// \param allowed Белый список допустимых имен без `--`.
    /// \throw std::invalid_argument Если встретилась неизвестная опция.
    void AllowSet(const std::set<std::string>& allowed) const {
        for (const auto& [key, value] : m_values) if (!allowed.contains(key)) throw std::invalid_argument("unknown --" + key);
    }
};

/// \brief Читает беззнаковое целое в заданном диапазоне.
/// \param text Текстовое представление числа без знака.
/// \param minimum Нижняя включительная граница.
/// \param maximum Верхняя включительная граница.
/// \return Разобранное значение.
/// \throw std::invalid_argument При ошибке разбора или выходе за диапазон.
inline std::uint64_t Number(std::string_view text, std::uint64_t minimum, std::uint64_t maximum) {
    std::uint64_t value{};
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value < minimum || value > maximum)
        throw std::invalid_argument("integer out of range");
    return value;
}

/// \brief Разбирает человекочитаемый размер или длительность с поддерживаемыми суффиксами.
/// \param text Значение вроде `20GiB`, `5m`, `1000ms`.
/// \param duration `true` для временных суффиксов (`ms`, `s`, `m`, `h`), `false` для size-суффиксов (`KiB`, `MiB`, `GiB`, `TiB`).
/// \return Значение в базовых единицах: байты либо миллисекунды.
/// \throw std::invalid_argument При неизвестном формате или выходе за диапазон.
inline std::uint64_t Quantity(std::string text, bool duration = false) {
    std::uint64_t multiplier{1};
    static constexpr std::array<std::pair<std::string_view, std::uint64_t>, 4> duration_units{{
        {"ms", 1},
        {"s", 1000},
        {"m", 60000},
        {"h", 3600000},
    }};
    static constexpr std::array<std::pair<std::string_view, std::uint64_t>, 4> size_units{{
        {"KiB", 1ULL << 10},
        {"MiB", 1ULL << 20},
        {"GiB", 1ULL << 30},
        {"TiB", 1ULL << 40},
    }};
    const auto& units = duration ? duration_units : size_units;
    for (const auto& [suffix, factor] : units) if (text.ends_with(suffix)) {
        text.resize(text.size() - suffix.size()); multiplier = factor; break;
    }
    return Number(text, 1, UINT64_MAX / multiplier) * multiplier;
}
} // namespace cybou::cli
#endif
