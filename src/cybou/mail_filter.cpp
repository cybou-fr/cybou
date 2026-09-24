// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/mail_filter.h>

#include <blockfilter.h>
#include <crypto/sha256.h>

#include <algorithm>
#include <string_view>

namespace cybou {

namespace {

inline void AppendUint32LE(std::vector<unsigned char>& out, uint32_t val)
{
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<unsigned char>(val >> (8 * i)));
    }
}

inline uint32_t ReadUint32LE(const std::span<const unsigned char>& bytes, size_t offset)
{
    uint32_t val{0};
    for (int i = 0; i < 4; ++i) {
        val |= uint32_t{bytes[offset + i]} << (8 * i);
    }
    return val;
}

} // namespace

uint256 ComputeRecipientDiscoveryTag(const uint256& recipient_key, const uint256& salt)
{
    static constexpr std::string_view DOMAIN{"CYBOU/DISCOVERY_TAG/V2"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(recipient_key.begin(), recipient_key.size());
    hasher.Write(salt.begin(), salt.size());
    uint256 tag;
    hasher.Finalize(tag.begin());
    return tag;
}

bool CybouMailDiscoveryFilter::Match(const uint256& discovery_tag) const
{
    if (num_elements == 0 || encoded_filter.empty()) {
        return false;
    }
    GCSFilter::Params params(block_id.GetUint64(0), block_id.GetUint64(1), GCS_PARAM_P, GCS_PARAM_M);
    try {
        GCSFilter gcs(params, encoded_filter, false);
        GCSFilter::Element elem(discovery_tag.begin(), discovery_tag.end());
        return gcs.Match(elem);
    } catch (...) {
        return false;
    }
}

bool CybouMailDiscoveryFilter::MatchAny(std::span<const uint256> discovery_tags) const
{
    if (num_elements == 0 || encoded_filter.empty() || discovery_tags.empty()) {
        return false;
    }
    GCSFilter::Params params(block_id.GetUint64(0), block_id.GetUint64(1), GCS_PARAM_P, GCS_PARAM_M);
    try {
        GCSFilter gcs(params, encoded_filter, false);
        GCSFilter::ElementSet elements;
        elements.reserve(discovery_tags.size());
        for (const auto& tag : discovery_tags) {
            elements.emplace(tag.begin(), tag.end());
        }
        return gcs.MatchAny(elements);
    } catch (...) {
        return false;
    }
}

uint256 CybouMailDiscoveryFilter::ComputeFilterHash() const
{
    static constexpr std::string_view DOMAIN{"CYBOU/MAIL_FILTER/V2"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(block_id.begin(), block_id.size());

    unsigned char count_bytes[4];
    for (int i = 0; i < 4; ++i) {
        count_bytes[i] = static_cast<unsigned char>(num_elements >> (8 * i));
    }
    hasher.Write(count_bytes, sizeof(count_bytes));
    hasher.Write(encoded_filter.data(), encoded_filter.size());

    uint256 hash;
    hasher.Finalize(hash.begin());
    return hash;
}

uint256 CybouMailDiscoveryFilter::ComputeFilterHeader(const uint256& prev_filter_header) const
{
    static constexpr std::string_view DOMAIN{"CYBOU/MAIL_FILTER_HEADER/V2"};
    CSHA256 hasher;
    hasher.Write(reinterpret_cast<const unsigned char*>(DOMAIN.data()), DOMAIN.size());
    hasher.Write(prev_filter_header.begin(), prev_filter_header.size());
    const uint256 filter_hash = ComputeFilterHash();
    hasher.Write(filter_hash.begin(), filter_hash.size());
    uint256 header;
    hasher.Finalize(header.begin());
    return header;
}

CybouMailDiscoveryFilter BuildMailDiscoveryFilter(const uint256& block_id, std::span<const uint256> discovery_tags)
{
    GCSFilter::Params params(block_id.GetUint64(0), block_id.GetUint64(1), GCS_PARAM_P, GCS_PARAM_M);
    GCSFilter::ElementSet elements;
    elements.reserve(discovery_tags.size());
    for (const auto& tag : discovery_tags) {
        elements.emplace(tag.begin(), tag.end());
    }

    GCSFilter gcs(params, elements);

    CybouMailDiscoveryFilter filter;
    filter.version = MAIL_DISCOVERY_FILTER_VERSION;
    filter.block_id = block_id;
    filter.num_elements = gcs.GetN();
    filter.encoded_filter = gcs.GetEncoded();
    return filter;
}

CybouMailDiscoveryFilter BuildBlockMailDiscoveryFilter(const CybouBlock& block)
{
    const uint256 block_id = ComputeBlockId(block);
    std::vector<uint256> tags;
    for (const auto& op : block.operations) {
        if (std::holds_alternative<AuthorizedMail>(op)) {
            const auto& mail_op = std::get<AuthorizedMail>(op);
            tags.push_back(mail_op.mail.discovery_tag);
        }
    }
    return BuildMailDiscoveryFilter(block_id, tags);
}

std::vector<unsigned char> SerializeMailDiscoveryFilter(const CybouMailDiscoveryFilter& filter)
{
    std::vector<unsigned char> out;
    out.reserve(1 + 32 + 4 + 4 + filter.encoded_filter.size());

    out.push_back(filter.version);
    out.insert(out.end(), filter.block_id.begin(), filter.block_id.end());
    AppendUint32LE(out, filter.num_elements);
    AppendUint32LE(out, static_cast<uint32_t>(filter.encoded_filter.size()));
    out.insert(out.end(), filter.encoded_filter.begin(), filter.encoded_filter.end());

    return out;
}

std::optional<CybouMailDiscoveryFilter> DeserializeMailDiscoveryFilter(std::span<const unsigned char> bytes)
{
    static constexpr size_t HEADER_SIZE{1 + 32 + 4 + 4}; // 41 bytes
    if (bytes.size() < HEADER_SIZE) {
        return std::nullopt;
    }
    if (bytes[0] != MAIL_DISCOVERY_FILTER_VERSION) {
        return std::nullopt;
    }

    CybouMailDiscoveryFilter filter;
    filter.version = bytes[0];

    size_t offset{1};
    std::copy_n(bytes.begin() + offset, 32, filter.block_id.begin());
    offset += 32;

    filter.num_elements = ReadUint32LE(bytes, offset);
    offset += 4;

    const uint32_t filter_len = ReadUint32LE(bytes, offset);
    offset += 4;

    if (bytes.size() != HEADER_SIZE + filter_len) {
        return std::nullopt;
    }

    filter.encoded_filter.assign(bytes.begin() + offset, bytes.end());
    return filter;
}

} // namespace cybou
