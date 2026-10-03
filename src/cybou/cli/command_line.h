// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license.
#ifndef CYBOU_CLI_COMMAND_LINE_H
#define CYBOU_CLI_COMMAND_LINE_H
#include <charconv>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cybou::cli {
class Options {
    std::map<std::string, std::string> m_values;
public:
    Options(int argc, char* argv[], int start) {
        for (int i = start; i < argc; ++i) {
            std::string key = argv[i];
            if (!key.starts_with("--") || key.size() <= 2 || i + 1 >= argc ||
                std::string_view{argv[i + 1]}.starts_with("--")) throw std::invalid_argument("expected --option value");
            if (!m_values.emplace(key.substr(2), argv[++i]).second) throw std::invalid_argument("duplicate option");
        }
    }
    bool Has(const std::string& key) const { return m_values.contains(key); }
    std::string Get(const std::string& key, const std::string& fallback = {}) const {
        auto it = m_values.find(key); return it == m_values.end() ? fallback : it->second;
    }
    std::string Require(const std::string& key) const {
        auto value = Get(key); if (value.empty()) throw std::invalid_argument("missing --" + key); return value;
    }
    void Allow(std::initializer_list<std::string> keys) const { AllowSet(std::set<std::string>(keys)); }
    void AllowSet(const std::set<std::string>& allowed) const {
        for (const auto& [key, value] : m_values) if (!allowed.contains(key)) throw std::invalid_argument("unknown --" + key);
    }
};
inline std::uint64_t Number(std::string_view text, std::uint64_t minimum, std::uint64_t maximum) {
    std::uint64_t value{};
    auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || value < minimum || value > maximum)
        throw std::invalid_argument("integer out of range");
    return value;
}
inline std::uint64_t Quantity(std::string text, bool duration = false) {
    std::uint64_t multiplier{1};
    const std::map<std::string, std::uint64_t> units = duration
        ? std::map<std::string, std::uint64_t>{{"ms",1},{"s",1000},{"m",60000},{"h",3600000}}
        : std::map<std::string, std::uint64_t>{{"KiB",1ULL<<10},{"MiB",1ULL<<20},{"GiB",1ULL<<30},{"TiB",1ULL<<40}};
    for (const auto& [suffix, factor] : units) if (text.ends_with(suffix)) {
        text.resize(text.size() - suffix.size()); multiplier = factor; break;
    }
    return Number(text, 1, UINT64_MAX / multiplier) * multiplier;
}
} // namespace cybou::cli
#endif
