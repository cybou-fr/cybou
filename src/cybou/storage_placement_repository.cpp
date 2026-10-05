// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Encrypted placement records and atomic placement index persistence.

#include <cybou/storage_service_internal.h>
#include <algorithm>

namespace cybou {

namespace {
constexpr std::array<unsigned char, 4> MAGIC{'C', 'Y', 'S', 'P'};

/// Ключ индекса всех placements, за которыми идёт audit/repair.
constexpr std::string_view PLACEMENT_INDEX_KEY{"storage/placements"};

/// Placement metadata живут в Application DB, а не в consensus state.
std::string PlacementKey(const cybou::Hash256& operation_id)
{
    return "storage/placement/" + operation_id.GetHex();
}

/// Little-endian encoding достаточно для локального state.
void Append16(std::vector<unsigned char>& out, const std::uint16_t value)
{
    out.push_back(static_cast<unsigned char>(value));
    out.push_back(static_cast<unsigned char>(value >> 8));
}

/// Little-endian encoding достаточно для локального state.
void Append32(std::vector<unsigned char>& out, const std::uint32_t value)
{
    for (unsigned i{0}; i < 4; ++i) out.push_back(static_cast<unsigned char>(value >> (8 * i)));
}

/// Минимальный локальный reader fail-closed для placement metadata.
class Reader {
public:
    explicit Reader(std::span<const unsigned char> bytes) : m_bytes{bytes} {}
    bool Take(std::span<unsigned char> out)
    {
        if (m_bytes.size() - m_offset < out.size()) return false;
        std::copy_n(m_bytes.begin() + m_offset, out.size(), out.begin());
        m_offset += out.size();
        return true;
    }
    std::optional<std::uint32_t> U8()
    {
        std::array<unsigned char, 1> b{};
        return Take(b) ? std::optional<std::uint32_t>{b[0]} : std::nullopt;
    }
    std::optional<std::uint32_t> U16()
    {
        std::array<unsigned char, 2> b{};
        if (!Take(b)) return std::nullopt;
        return std::uint32_t{b[0]} | (std::uint32_t{b[1]} << 8);
    }
    std::optional<std::uint32_t> U32()
    {
        std::array<unsigned char, 4> b{};
        if (!Take(b)) return std::nullopt;
        std::uint32_t value{0};
        for (unsigned i{0}; i < 4; ++i) value |= std::uint32_t{b[i]} << (8 * i);
        return value;
    }
    bool Done() const { return m_offset == m_bytes.size(); }

private:
    std::span<const unsigned char> m_bytes;
    std::size_t m_offset{0};
};

} // namespace

std::optional<StorageService::Placement> StorageService::PlacementRepository::Load(const cybou::Hash256& operation_id, const bool rebuilding) const
{
    const auto encoded = m_application_db.Get(rebuilding ? "storage/rebuild/"+operation_id.GetHex() : PlacementKey(operation_id));
    if (!encoded) return std::nullopt;
    Reader in{*encoded};
    std::array<unsigned char, MAGIC.size()> magic{};
    Placement placement;
    if (!in.Take(magic) || magic != MAGIC || !in.Take(std::span{placement.operation_id.begin(), 32}) ||
        placement.operation_id != operation_id) return std::nullopt;
    const auto count = in.U32();
    if (!count || *count == 0 || *count > MAX_PUBLICATION_CHUNKS) return std::nullopt;
    placement.leaves.resize(*count);
    placement.replicas.resize(*count);
    for (std::uint32_t i{0}; i < *count; ++i) {
        if (!in.Take(placement.leaves[i])) return std::nullopt;
        const auto replicas = in.U8();
        if (!replicas || *replicas > MAX_REPLICAS_PER_CHUNK) return std::nullopt;
        placement.replicas[i].reserve(*replicas);
        for (std::uint32_t r{0}; r < *replicas; ++r) {
            StorageEndpoint endpoint;
            if (!in.Take(endpoint.storage_id)) return std::nullopt;
            const auto length = in.U8();
            if (!length || *length == 0) return std::nullopt;
            std::string address(*length, '\0');
            if (!in.Take(std::span{reinterpret_cast<unsigned char*>(address.data()), address.size()})) {
                return std::nullopt;
            }
            const auto port = in.U16();
            if (!port || *port == 0) return std::nullopt;
            endpoint.address = std::move(address);
            endpoint.port = static_cast<std::uint16_t>(*port);
            if (HasProvider(placement.replicas[i], endpoint)) return std::nullopt;
            placement.replicas[i].push_back(std::move(endpoint));
        }
    }
    if (!in.Done()) return std::nullopt;
    return placement;
}

bool StorageService::PlacementRepository::Save(const Placement& placement, const bool rebuilding)
{
    std::size_t reserve = MAGIC.size() + placement.operation_id.size() + 4;
    for (const auto& replicas : placement.replicas) {
        reserve += 32 + 1;
        for (std::size_t r{0}; r < replicas.size() && r < MAX_REPLICAS_PER_CHUNK; ++r) {
            reserve += 32 + 1 + replicas[r].address.size() + 2;
        }
    }
    std::vector<unsigned char> out;
    out.reserve(reserve);
    out.insert(out.end(), MAGIC.begin(), MAGIC.end());
    out.insert(out.end(), placement.operation_id.begin(), placement.operation_id.end());
    Append32(out, static_cast<std::uint32_t>(placement.leaves.size()));
    for (std::size_t i{0}; i < placement.leaves.size(); ++i) {
        out.insert(out.end(), placement.leaves[i].begin(), placement.leaves[i].end());
        const auto& replicas = placement.replicas[i];
        out.push_back(static_cast<unsigned char>(std::min(replicas.size(), MAX_REPLICAS_PER_CHUNK)));
        for (std::size_t r{0}; r < replicas.size() && r < MAX_REPLICAS_PER_CHUNK; ++r) {
            const auto& address = replicas[r].address;
            if (address.empty() || address.size() > 255) return false;
            out.insert(out.end(), replicas[r].storage_id.begin(), replicas[r].storage_id.end());
            out.push_back(static_cast<unsigned char>(address.size()));
            out.insert(out.end(), address.begin(), address.end());
            Append16(out, replicas[r].port);
        }
    }
    // Placement и его присутствие в индексе должны фиксироваться как одна логическая запись.
    if (rebuilding) return m_application_db.Put("storage/rebuild/"+placement.operation_id.GetHex(), out);
    PrivateApplicationStore::Batch batch{m_application_db};
    if (!m_application_db.Put(PlacementKey(placement.operation_id), out)) return false;
    auto index = PlacementIndex();
    if (std::find(index.begin(), index.end(), placement.operation_id) == index.end()) {
        std::vector<unsigned char> encoded;
        for (const auto& id : index) encoded.insert(encoded.end(), id.begin(), id.end());
        encoded.insert(encoded.end(), placement.operation_id.begin(), placement.operation_id.end());
        if (!m_application_db.Put(PLACEMENT_INDEX_KEY, encoded)) return false;
    }
    return batch.Commit();
}

std::vector<cybou::Hash256> StorageService::PlacementRepository::PlacementIndex() const
{
    std::vector<cybou::Hash256> ids;
    const auto encoded = m_application_db.Get(PLACEMENT_INDEX_KEY);
    if (!encoded || encoded->size() % 32 != 0) return ids;
    for (std::size_t offset{0}; offset < encoded->size(); offset += 32) {
        cybou::Hash256 id;
        std::copy_n(encoded->begin() + static_cast<std::ptrdiff_t>(offset), 32, id.begin());
        ids.push_back(id);
    }
    return ids;
}

} // namespace cybou
