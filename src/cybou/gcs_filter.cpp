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

std::vector<uint64_t> DecodeValues(const Params& params, const std::span<const unsigned char> encoded)
{
    SpanReader stream{encoded};
    const uint64_t count = ReadCompactSize(stream);
    if (count > std::numeric_limits<uint32_t>::max()) throw std::ios_base::failure("GCS element count exceeds uint32");

    std::vector<uint64_t> values;
    values.reserve(static_cast<size_t>(count));
    BitStreamReader bitreader{stream};
    uint64_t value{0};
    for (uint64_t i = 0; i < count; ++i) {
        const uint64_t delta = GolombRiceDecode(bitreader, params.p);
        if (delta > std::numeric_limits<uint64_t>::max() - value) throw std::ios_base::failure("GCS value overflow");
        value += delta;
        values.push_back(value);
    }
    if (!stream.empty()) throw std::ios_base::failure("encoded GCS filter contains excess data");
    return values;
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
    const auto values = DecodeValues(params, encoded);
    if (values.size() > std::numeric_limits<uint32_t>::max()) throw std::ios_base::failure("GCS element count exceeds uint32");
    return static_cast<uint32_t>(values.size());
}

bool Match(const Params& params, const std::span<const unsigned char> encoded, const std::span<const unsigned char> element)
{
    const auto values = DecodeValues(params, encoded);
    if (values.empty()) return false;
    const uint64_t range = static_cast<uint64_t>(values.size()) * params.m;
    const uint64_t query = HashToRange(params, range, element);
    return std::binary_search(values.begin(), values.end(), query);
}

bool MatchAny(const Params& params, const std::span<const unsigned char> encoded, const std::span<const Element> elements)
{
    const auto values = DecodeValues(params, encoded);
    if (values.empty() || elements.empty()) return false;
    const uint64_t range = static_cast<uint64_t>(values.size()) * params.m;
    const auto queries = HashElements(params, range, elements);

    size_t value_index{0};
    size_t query_index{0};
    while (value_index < values.size() && query_index < queries.size()) {
        if (values[value_index] == queries[query_index]) return true;
        if (values[value_index] < queries[query_index]) ++value_index;
        else ++query_index;
    }
    return false;
}

} // namespace cybou::gcs
