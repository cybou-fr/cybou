// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/gcs_filter.h>
#include <cybou/fast_range.h>

#include <crypto/siphash.h>

#include <algorithm>
#include <ios>
#include <limits>
#include <stdexcept>

namespace cybou::gcs {
namespace {

constexpr uint64_t MAX_COMPACT_SIZE{0x02000000};

class ByteReader final
{
public:
    explicit ByteReader(const std::span<const unsigned char> bytes) : m_bytes{bytes} {}

    unsigned char ReadByte()
    {
        if (m_offset == m_bytes.size()) throw std::ios_base::failure("truncated GCS filter");
        return m_bytes[m_offset++];
    }

    bool Empty() const { return m_offset == m_bytes.size(); }

private:
    std::span<const unsigned char> m_bytes;
    size_t m_offset{0};
};

class BitReader final
{
public:
    explicit BitReader(ByteReader& reader) : m_reader{reader} {}

    uint64_t Read(int bits)
    {
        if (bits < 0 || bits > 64) throw std::out_of_range("GCS bit count must be between 0 and 64");
        uint64_t value{0};
        while (bits > 0) {
            if (m_offset == 8) {
                m_buffer = m_reader.ReadByte();
                m_offset = 0;
            }
            const int take = std::min(8 - m_offset, bits);
            value <<= take;
            value |= static_cast<uint8_t>(m_buffer << m_offset) >> (8 - take);
            m_offset += take;
            bits -= take;
        }
        return value;
    }

private:
    ByteReader& m_reader;
    uint8_t m_buffer{0};
    int m_offset{8};
};

class BitWriter final
{
public:
    explicit BitWriter(std::vector<unsigned char>& output) : m_output{output} {}

    void Write(uint64_t value, int bits)
    {
        if (bits < 0 || bits > 64) throw std::out_of_range("GCS bit count must be between 0 and 64");
        while (bits > 0) {
            const int take = std::min(8 - m_offset, bits);
            m_buffer |= (value << (64 - bits)) >> (64 - 8 + m_offset);
            m_offset += take;
            bits -= take;
            if (m_offset == 8) Flush();
        }
    }

    void Flush()
    {
        if (m_offset == 0) return;
        m_output.push_back(m_buffer);
        m_buffer = 0;
        m_offset = 0;
    }

private:
    std::vector<unsigned char>& m_output;
    uint8_t m_buffer{0};
    int m_offset{0};
};

void WriteCompactSize(std::vector<unsigned char>& output, uint64_t value)
{
    if (value < 253) {
        output.push_back(static_cast<unsigned char>(value));
        return;
    }
    const unsigned char marker = value <= std::numeric_limits<uint16_t>::max() ? 253 :
        value <= std::numeric_limits<uint32_t>::max() ? 254 : 255;
    output.push_back(marker);
    const int bytes = marker == 253 ? 2 : marker == 254 ? 4 : 8;
    for (int i = 0; i < bytes; ++i) output.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

uint64_t ReadCompactSize(ByteReader& reader)
{
    const unsigned char marker = reader.ReadByte();
    if (marker < 253) return marker;
    const int bytes = marker == 253 ? 2 : marker == 254 ? 4 : 8;
    uint64_t value{0};
    for (int i = 0; i < bytes; ++i) value |= uint64_t{reader.ReadByte()} << (8 * i);
    if ((marker == 253 && value < 253) ||
        (marker == 254 && value <= std::numeric_limits<uint16_t>::max()) ||
        (marker == 255 && value <= std::numeric_limits<uint32_t>::max())) {
        throw std::ios_base::failure("non-canonical GCS element count");
    }
    return value;
}

void GolombRiceEncode(BitWriter& writer, const uint8_t p, const uint64_t value)
{
    if (p >= 64) throw std::out_of_range("GCS Golomb-Rice parameter must be below 64");
    uint64_t quotient = value >> p;
    while (quotient != 0) {
        const int ones = quotient <= 64 ? static_cast<int>(quotient) : 64;
        writer.Write(std::numeric_limits<uint64_t>::max(), ones);
        quotient -= static_cast<uint64_t>(ones);
    }
    writer.Write(0, 1);
    writer.Write(value, p);
}

uint64_t GolombRiceDecode(BitReader& reader, const uint8_t p)
{
    if (p >= 64) throw std::out_of_range("GCS Golomb-Rice parameter must be below 64");
    uint64_t quotient{0};
    while (reader.Read(1) != 0) {
        if (quotient == std::numeric_limits<uint64_t>::max() >> p) {
            throw std::ios_base::failure("GCS Golomb-Rice value overflow");
        }
        ++quotient;
    }
    const uint64_t remainder = reader.Read(p);
    return (quotient << p) + remainder;
}

uint64_t HashToRange(const Params& params, const uint64_t range, const std::span<const unsigned char> element)
{
    const uint64_t hash = CSipHasher{params.siphash_k0, params.siphash_k1}.Write(element).Finalize();
    return cybou::FastRange64(hash, range);
}

uint32_t ReadCount(const Params& params, ByteReader& stream)
{
    const uint64_t count = ReadCompactSize(stream);
    if (count > MAX_COMPACT_SIZE) throw std::ios_base::failure("GCS element count exceeds serialization limit");
    if (count > std::numeric_limits<uint32_t>::max()) throw std::ios_base::failure("GCS element count exceeds uint32");
    if (count > params.max_elements) throw std::ios_base::failure("GCS element count exceeds configured limit");
    return static_cast<uint32_t>(count);
}

template <typename Visitor>
uint32_t ForEachValue(const Params& params, const std::span<const unsigned char> encoded, Visitor&& visitor)
{
    ByteReader stream{encoded};
    const uint32_t count = ReadCount(params, stream);
    BitReader bitreader{stream};
    uint64_t value{0};
    for (uint32_t i = 0; i < count; ++i) {
        const uint64_t delta = GolombRiceDecode(bitreader, params.p);
        if (delta > std::numeric_limits<uint64_t>::max() - value) throw std::ios_base::failure("GCS value overflow");
        value += delta;
        visitor(value);
    }
    if (!stream.Empty()) throw std::ios_base::failure("encoded GCS filter contains excess data");
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
    WriteCompactSize(encoded, count);
    if (hashes.empty()) return encoded;

    BitWriter bitwriter{encoded};
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
    ByteReader count_stream{encoded};
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
    ByteReader count_stream{encoded};
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
