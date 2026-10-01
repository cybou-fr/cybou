// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_store.h>

#include <cybou/crypto/sha256.h>

#include <openssl/crypto.h>
#include <openssl/rand.h>

#include <algorithm>
#include <array>
#include <string>
#include <system_error>
#include <vector>

namespace cybou {
namespace {

constexpr uint32_t STORE_VERSION{1};
constexpr size_t ACTIVATION_CODE_MIN_BYTES{32};
constexpr size_t ACTIVATION_CODE_MAX_BYTES{128};
constexpr std::string_view ACTIVATION_DOMAIN{"CYBOU/BOOTSTRAP/ACTIVATION-CODE/v1"};
constexpr std::string_view VERSION_KEY{"bootstrap/store-version"};
constexpr std::string_view STATE_KEY{"bootstrap/state"};
constexpr std::string_view ACTIVATION_HASH_KEY{"bootstrap/activation-sha256"};
constexpr std::string_view BINDING_KEY{"bootstrap/current-binding"};
constexpr std::string_view ARCHIVE_PREFIX{"bootstrap/archive/"};

std::string ArchiveKey(const uint64_t generation)
{
    return std::string{ARCHIVE_PREFIX} + std::to_string(generation);
}

std::optional<std::array<unsigned char, 32>> HashActivationCode(const std::string_view code)
{
    if (code.size() < ACTIVATION_CODE_MIN_BYTES || code.size() > ACTIVATION_CODE_MAX_BYTES) return std::nullopt;
    std::array<unsigned char, 32> digest{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(ACTIVATION_DOMAIN), crypto::Sha256Bytes(code)}, digest.data())) {
        return std::nullopt;
    }
    return digest;
}

std::string Hex(const std::span<const unsigned char> bytes)
{
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }
    return result;
}

} // namespace

BootstrapStore::BootstrapStore(std::unique_ptr<KVStore> database, const BootstrapStoreState state,
    const std::array<unsigned char, 32> activation_code_hash,
    std::optional<BootstrapNetworkBinding> binding)
    : m_database{std::move(database)}, m_state{state}, m_activation_code_hash{activation_code_hash},
      m_binding{std::move(binding)}
{
}

std::optional<BootstrapProvision> BootstrapStore::Provision(const std::filesystem::path& path)
{
    const auto generated_code = GenerateActivationCode();
    if (!generated_code) return std::nullopt;
    const std::string_view one_use_activation_code{*generated_code};
    const auto activation_hash = HashActivationCode(one_use_activation_code);
    if (!activation_hash || path.empty()) return std::nullopt;
    try {
        std::error_code ec;
        const auto parent = path.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, ec);
        if (ec || std::filesystem::exists(path, ec) || ec) return std::nullopt;
        if (!std::filesystem::create_directory(path, ec) || ec) return std::nullopt;

        auto database = std::make_unique<KVStore>(KVStoreOptions{.path = path});
        KVStore::Batch batch;
        batch.Write(std::string{VERSION_KEY}, STORE_VERSION);
        batch.Write(std::string{STATE_KEY}, static_cast<uint8_t>(BootstrapStoreState::EMPTY));
        batch.Write(std::string{ACTIVATION_HASH_KEY},
            std::vector<unsigned char>(activation_hash->begin(), activation_hash->end()));
        database->WriteBatch(batch, true);
        BootstrapProvision provision{
            std::unique_ptr<BootstrapStore>{new BootstrapStore{std::move(database),
                BootstrapStoreState::EMPTY, *activation_hash, std::nullopt}},
            *generated_code};
        return provision;
    } catch (...) {
        return std::nullopt;
    }
}

std::unique_ptr<BootstrapStore> BootstrapStore::Open(const std::filesystem::path& path)
{
    try {
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec) || ec) return nullptr;
        auto database = std::make_unique<KVStore>(KVStoreOptions{.path = path});
        uint32_t version{0};
        uint8_t stored_state{0};
        if (!database->Read(std::string{VERSION_KEY}, version) || version != STORE_VERSION ||
            !database->Read(std::string{STATE_KEY}, stored_state) || stored_state > 1) return nullptr;

        std::vector<unsigned char> activation_hash_bytes;
        const bool have_activation_hash = database->Read(std::string{ACTIVATION_HASH_KEY}, activation_hash_bytes);
        const bool have_binding_bytes = database->Exists(std::string{BINDING_KEY});
        if (stored_state == static_cast<uint8_t>(BootstrapStoreState::EMPTY)) {
            if (!have_activation_hash || activation_hash_bytes.size() != 32 || have_binding_bytes) return nullptr;
            std::array<unsigned char, 32> activation_hash{};
            std::copy(activation_hash_bytes.begin(), activation_hash_bytes.end(), activation_hash.begin());
            return std::unique_ptr<BootstrapStore>{new BootstrapStore{std::move(database),
                BootstrapStoreState::EMPTY, activation_hash, std::nullopt}};
        }

        if (have_activation_hash || !have_binding_bytes) return nullptr;
        std::vector<unsigned char> binding_bytes;
        if (!database->Read(std::string{BINDING_KEY}, binding_bytes)) return nullptr;
        auto binding = DecodeBootstrapNetworkBinding(binding_bytes);
        if (!binding) return nullptr;
        std::array<unsigned char, 32> empty_hash{};
        return std::unique_ptr<BootstrapStore>{new BootstrapStore{std::move(database),
            BootstrapStoreState::BOUND, empty_hash, std::move(binding)}};
    } catch (...) {
        return nullptr;
    }
}

std::optional<std::string> BootstrapStore::GenerateActivationCode()
{
    std::array<unsigned char, 32> random{};
    if (RAND_bytes(random.data(), static_cast<int>(random.size())) != 1) return std::nullopt;
    return Hex(random);
}

BootstrapStoreState BootstrapStore::State() const
{
    std::lock_guard lock{m_mutex};
    return m_state;
}

std::optional<BootstrapNetworkBinding> BootstrapStore::CurrentBinding() const
{
    std::lock_guard lock{m_mutex};
    return m_binding;
}

std::optional<BootstrapNetworkBinding> BootstrapStore::ArchivedBinding(const uint64_t generation) const
{
    std::lock_guard lock{m_mutex};
    if (m_state != BootstrapStoreState::BOUND || !m_database->Exists(ArchiveKey(generation))) return std::nullopt;
    std::vector<unsigned char> encoded;
    if (!m_database->Read(ArchiveKey(generation), encoded)) return std::nullopt;
    auto binding = DecodeBootstrapNetworkBinding(encoded);
    if (!binding || binding->generation != generation) return std::nullopt;
    return binding;
}

BootstrapClaimStatus BootstrapStore::ClaimInitialNetwork(const std::string_view activation_code,
    const BootstrapNetworkBinding& binding)
{
    std::lock_guard lock{m_mutex};
    if (m_state != BootstrapStoreState::EMPTY) return BootstrapClaimStatus::ALREADY_BOUND;
    const auto supplied_hash = HashActivationCode(activation_code);
    if (!supplied_hash || CRYPTO_memcmp(supplied_hash->data(), m_activation_code_hash.data(),
            m_activation_code_hash.size()) != 0) return BootstrapClaimStatus::INVALID_ACTIVATION_CODE;
    if (binding.generation != 1 || !VerifyBootstrapNetworkBinding(binding)) {
        return BootstrapClaimStatus::INVALID_BINDING;
    }
    const auto encoded = EncodeBootstrapNetworkBinding(binding);
    if (!encoded) return BootstrapClaimStatus::INVALID_BINDING;

    try {
        KVStore::Batch batch;
        batch.Write(std::string{STATE_KEY}, static_cast<uint8_t>(BootstrapStoreState::BOUND));
        batch.Write(std::string{BINDING_KEY}, *encoded);
        batch.Erase(std::string{ACTIVATION_HASH_KEY});
        m_database->WriteBatch(batch, true);
    } catch (...) {
        return BootstrapClaimStatus::STORAGE_ERROR;
    }
    m_state = BootstrapStoreState::BOUND;
    m_binding = binding;
    OPENSSL_cleanse(m_activation_code_hash.data(), m_activation_code_hash.size());
    return BootstrapClaimStatus::CLAIMED;
}

BootstrapReplacementStatus BootstrapStore::ReplaceNetwork(
    const BootstrapNetworkReplacement& replacement)
{
    std::lock_guard lock{m_mutex};
    if (m_state != BootstrapStoreState::BOUND || !m_binding) return BootstrapReplacementStatus::NOT_BOUND;
    if (!VerifyBootstrapNetworkReplacement(*m_binding, replacement)) {
        return BootstrapReplacementStatus::INVALID_REPLACEMENT;
    }
    const auto encoded_current = EncodeBootstrapNetworkBinding(*m_binding);
    const auto encoded_next = EncodeBootstrapNetworkBinding(replacement.new_binding);
    if (!encoded_current || !encoded_next) return BootstrapReplacementStatus::INVALID_REPLACEMENT;
    const auto archive_key = ArchiveKey(m_binding->generation);
    if (m_database->Exists(archive_key)) return BootstrapReplacementStatus::ARCHIVE_CONFLICT;

    try {
        KVStore::Batch batch;
        batch.Write(archive_key, *encoded_current);
        batch.Write(std::string{BINDING_KEY}, *encoded_next);
        m_database->WriteBatch(batch, true);
    } catch (...) {
        return BootstrapReplacementStatus::STORAGE_ERROR;
    }
    m_binding = replacement.new_binding;
    return BootstrapReplacementStatus::REPLACED;
}

} // namespace cybou
