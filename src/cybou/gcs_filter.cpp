// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/gcs_filter.h>

#include <crypto/siphash.h>
#include <streams.h>
#include <util/fastrange.h>
#include <util/golombrice.h>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace cybou::gcs {
namespace {

uint64_t HashToRange(const Params& params, const uint64_t range, const std::span<const unsigned char> element)
{
    const uint64_t hash = CSipHasher{params.siphash_k0, params.siphash_k1}.Write(element).Finalize();
    return FastRange64(hash, range);
}

uint32_t ReadCount(const Params& params, SpanReader& stream)
{
    const uint64_t count = ReadCompactSize(stream);
    if (count > std::numeric_limits<uint32_t>::max()) throw std::ios_base::failure("GCS element count exceeds uint32");
    if (count > params.max_elements) throw std::ios_base::failure("GCS element count exceeds configured limit");
    return static_cast<uint32_t>(count);
}

template <typename Visitor>
uint32_t ForEachValue(const Params& params, const std::span<const unsigned char> encoded, Visitor&& visitor)
{
    SpanReader stream{encoded};
    const uint32_t count = ReadCount(params, stream);
    BitStreamReader bitreader{stream};
    uint64_t value{0};
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t delta = GolombRiceDecode(bitreader, params.p);
        if (delta > std::numeric_limits<uint64_t>::max() - value) throw std::ios_base::failure("GCS value overflow");
        value += delta;
        visitor(value);
    }
    if (!stream.empty()) throw std::ios_base::failure("encoded GCS filter contains excess data");
    return count;
}

std::vector<uint64_t> HashElements(const Params& params, const uint64_t range, std::span<const Element> elements)
{
    std::vector<uint64_t> hashes;
    hashes.reserve(elements.size());
    for (const auto& element : elements) hashes.push_back(HashToRange(params, range, element));
    std::sort(hashes.begin(), hashes.end());
    return hashes;
}

} // namespace

std::vector<unsigned char> Build(const Params& params, std::span<const Element> elements)
{
    std::vector<Element> unique_elements{elements.begin(), elements.end()};
    std::sort(unique_elements.begin(), unique_elements.end());
    unique_elements.erase(std::unique(unique_elements.begin(), unique_elements.end()), unique_elements.end());
    if (unique_elements.size() > std::numeric_limits<uint32_t>::max()) throw std::invalid_argument("GCS element count exceeds uint32");
    if (unique_elements.size() > params.max_elements) throw std::invalid_argument("GCS element count exceeds configured limit");

    const uint32_t count = static_cast<uint32_t>(unique_elements.size());
    const uint64_t range = static_cast<uint64_t>(count) * params.m;
    const auto hashes = HashElements(params, range, unique_elements);

    std::vector<unsigned char> encoded;
    VectorWriter stream{encoded, 0};
    WriteCompactSize(stream, count);
    if (hashes.empty()) return encoded;

    BitStreamWriter bitwriter{stream};
    uint64_t previous{0};
    for (const uint64_t hash : hashes) {
        GolombRiceEncode(bitwriter, params.p, hash - previous);
        previous = hash;
    }
    bitwriter.Flush();
    return encoded;
}

uint32_t ElementCount(const Params& params, const std::span<const unsigned char> encoded)
{
    return ForEachValue(params, encoded, [](uint64_t) {});
}

bool Match(const Params& params, const std::span<const unsigned char> encoded, const std::span<const unsigned char> element)
{
    SpanReader count_stream{encoded};
    const uint32_t count = ReadCount(params, count_stream);
    if (count == 0) {
        ForEachValue(params, encoded, [](uint64_t) {});
        return false;
    }
    const uint64_t range = static_cast<uint64_t>(count) * params.m;
    const uint64_t query = HashToRange(params, range, element);
    bool found{false};
    ForEachValue(params, encoded, [&](uint64_t value) { found = found || value == query; });
    return found;
}

bool MatchAny(const Params& params, const std::span<const unsigned char> encoded, const std::span<const Element> elements)
{
    SpanReader count_stream{encoded};
    const uint32_t count = ReadCount(params, count_stream);
    const uint64_t range = static_cast<uint64_t>(count) * params.m;
    const auto queries = count == 0 || elements.empty() ? std::vector<uint64_t>{} : HashElements(params, range, elements);
    size_t query_index{0};
    bool found{false};
    ForEachValue(params, encoded, [&](uint64_t value) {
        while (query_index < queries.size() && queries[query_index] < value) ++query_index;
        if (query_index < queries.size() && queries[query_index] == value) found = true;
    });
    return found;
}

} // namespace cybou::gcs
