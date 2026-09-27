// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#include <cybou/storage_key_ring.h>

#include <cybou/crypto/cleanse.h>
#include <cybou/identity_vault.h>

#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <limits>

namespace cybou {
namespace {
constexpr std::array<unsigned char, 5> MAGIC{'C', 'S', 'K', 'R', '1'};
constexpr size_t FIXED_SIZE{MAGIC.size() + AccountId::SIZE + sizeof(uint32_t)};
constexpr size_t EPOCH_SIZE{sizeof(uint32_t) + 32};

bool IsZero(std::span<const unsigned char> bytes)
{
    return std::all_of(bytes.begin(), bytes.end(), [](unsigned char value) { return value == 0; });
}

void Put32(unsigned char* out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = static_cast<unsigned char>(value >> (8 * i));
}

uint32_t Read32(const unsigned char* in)
{
    uint32_t value{0};
    for (unsigned i = 0; i < 4; ++i) value |= uint32_t{in[i]} << (8 * i);
    return value;
}
} // namespace

StorageKeyRing::StorageKeyRing(const AccountId account_id) : m_account_id{account_id} {}

StorageKeyRing::StorageKeyRing(StorageKeyRing&& other) noexcept
    : m_account_id{other.m_account_id}, m_epochs{std::move(other.m_epochs)}
{
    other.Clear();
}

StorageKeyRing& StorageKeyRing::operator=(StorageKeyRing&& other) noexcept
{
    if (this != &other) {
        Clear();
        m_account_id = other.m_account_id;
        m_epochs = std::move(other.m_epochs);
        other.Clear();
    }
    return *this;
}

StorageKeyRing::~StorageKeyRing() { Clear(); }

void StorageKeyRing::Clear() noexcept
{
    for (auto& epoch : m_epochs) crypto::CleanseMemory(epoch.key.data(), epoch.key.size());
    m_epochs.clear();
}

std::optional<StorageKeyRing> StorageKeyRing::Create(const AccountId account_id)
{
    if (account_id.IsNull()) return std::nullopt;
    StorageKeyRing ring{account_id};
    EpochKey first;
    if (RAND_bytes(first.key.data(), first.key.size()) != 1 || IsZero(first.key)) {
        crypto::CleanseMemory(first.key.data(), first.key.size());
        return std::nullopt;
    }
    ring.m_epochs.push_back(first);
    crypto::CleanseMemory(first.key.data(), first.key.size());
    return ring;
}

std::optional<std::vector<unsigned char>> StorageKeyRing::Serialize() const
{
    if (m_account_id.IsNull() || m_epochs.empty() || m_epochs.size() > STORAGE_KEY_RING_MAX_EPOCHS ||
        m_epochs.size() > (std::numeric_limits<size_t>::max() - FIXED_SIZE) / EPOCH_SIZE) return std::nullopt;
    std::vector<unsigned char> bytes(FIXED_SIZE + m_epochs.size() * EPOCH_SIZE);
    std::copy(MAGIC.begin(), MAGIC.end(), bytes.begin());
    std::copy(m_account_id.Value().begin(), m_account_id.Value().end(), bytes.begin() + MAGIC.size());
    Put32(bytes.data() + MAGIC.size() + AccountId::SIZE, static_cast<uint32_t>(m_epochs.size()));
    size_t offset{FIXED_SIZE};
    for (size_t i = 0; i < m_epochs.size(); ++i) {
        const auto& entry = m_epochs[i];
        if (entry.epoch != i || IsZero(entry.key)) {
            crypto::CleanseMemory(bytes.data(), bytes.size());
            return std::nullopt;
        }
        Put32(bytes.data() + offset, entry.epoch);
        offset += 4;
        std::copy(entry.key.begin(), entry.key.end(), bytes.begin() + offset);
        offset += entry.key.size();
    }
    return bytes;
}

std::optional<StorageKeyRing> StorageKeyRing::Parse(const std::span<const unsigned char> payload)
{
    if (payload.size() < FIXED_SIZE || !std::equal(MAGIC.begin(), MAGIC.end(), payload.begin())) return std::nullopt;
    const auto account = AccountId::FromBytes(payload.subspan(MAGIC.size(), AccountId::SIZE));
    const uint32_t count = Read32(payload.data() + MAGIC.size() + AccountId::SIZE);
    if (!account || count == 0 || count > STORAGE_KEY_RING_MAX_EPOCHS ||
        payload.size() != FIXED_SIZE + static_cast<size_t>(count) * EPOCH_SIZE) return std::nullopt;
    StorageKeyRing ring{*account};
    ring.m_epochs.reserve(count);
    size_t offset{FIXED_SIZE};
    for (uint32_t i = 0; i < count; ++i) {
        EpochKey entry;
        entry.epoch = Read32(payload.data() + offset);
        offset += 4;
        std::copy_n(payload.begin() + offset, entry.key.size(), entry.key.begin());
        offset += entry.key.size();
        if (entry.epoch != i || IsZero(entry.key)) {
            crypto::CleanseMemory(entry.key.data(), entry.key.size());
            return std::nullopt;
        }
        ring.m_epochs.push_back(entry);
        crypto::CleanseMemory(entry.key.data(), entry.key.size());
    }
    return ring;
}

bool StorageKeyRing::SaveNewToFile(const std::filesystem::path& path, const std::string_view password) const
{
    auto payload = Serialize();
    if (!payload) return false;
    const bool saved = SaveNewIdentityVault(path, password, *payload);
    crypto::CleanseMemory(payload->data(), payload->size());
    return saved;
}

std::optional<StorageKeyRing> StorageKeyRing::LoadFromFile(
    const std::filesystem::path& path, const std::string_view password)
{
    auto payload = LoadIdentityVault(path, password);
    if (!payload) return std::nullopt;
    auto ring = Parse(*payload);
    crypto::CleanseMemory(payload->data(), payload->size());
    return ring;
}

bool StorageKeyRing::RotateAndSave(const std::filesystem::path& path, const std::string_view password)
{
    if (m_epochs.empty() || m_epochs.size() >= STORAGE_KEY_RING_MAX_EPOCHS ||
        m_epochs.back().epoch == std::numeric_limits<uint32_t>::max()) return false;
    auto expected = Serialize();
    if (!expected) return false;

    StorageKeyRing candidate{m_account_id};
    candidate.m_epochs = m_epochs;
    EpochKey next{.epoch = m_epochs.back().epoch + 1};
    if (RAND_bytes(next.key.data(), next.key.size()) != 1 || IsZero(next.key)) {
        crypto::CleanseMemory(next.key.data(), next.key.size());
        crypto::CleanseMemory(expected->data(), expected->size());
        return false;
    }
    candidate.m_epochs.push_back(next);
    crypto::CleanseMemory(next.key.data(), next.key.size());
    auto replacement = candidate.Serialize();
    if (!replacement) {
        crypto::CleanseMemory(expected->data(), expected->size());
        return false;
    }
    const bool saved = ReplaceIdentityVault(path, password, *expected, *replacement);
    crypto::CleanseMemory(expected->data(), expected->size());
    crypto::CleanseMemory(replacement->data(), replacement->size());
    if (!saved) return false;
    Clear();
    m_epochs = std::move(candidate.m_epochs);
    candidate.Clear();
    return true;
}

uint32_t StorageKeyRing::CurrentEpoch() const
{
    return m_epochs.empty() ? 0 : m_epochs.back().epoch;
}

bool StorageKeyRing::CopyMasterKey(const uint32_t epoch, const std::span<unsigned char, 32> out) const
{
    if (m_epochs.empty() || epoch >= m_epochs.size() || m_epochs[epoch].epoch != epoch) return false;
    std::copy(m_epochs[epoch].key.begin(), m_epochs[epoch].key.end(), out.begin());
    return true;
}

} // namespace cybou
