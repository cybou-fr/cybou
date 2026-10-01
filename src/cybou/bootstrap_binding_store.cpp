// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#include <cybou/bootstrap_binding_store.h>

#include <system_error>
#include <vector>

namespace cybou {
namespace {

constexpr std::string_view BINDING_KEY{"bootstrap/client-binding"};

std::optional<IdentityHybridPublicKey> AuthorityKey(const BootstrapNetworkBinding& binding)
{
    const auto network = DeserializeCybouNetworkFile(binding.network_file);
    if (!network) return std::nullopt;
    return network->definition.poa_finalizer_public_key;
}

} // namespace

BootstrapBindingStore::BootstrapBindingStore(std::unique_ptr<KVStore> database,
    BootstrapNetworkBinding binding)
    : m_database{std::move(database)}, m_binding{std::move(binding)}
{
}

std::unique_ptr<BootstrapBindingStore> BootstrapBindingStore::TrustInitial(
    const std::filesystem::path& path, const BootstrapNetworkBinding& binding)
{
    if (path.empty() || !VerifyBootstrapNetworkBinding(binding)) return nullptr;
    try {
        std::error_code ec;
        const auto parent = path.parent_path();
        if (!parent.empty()) std::filesystem::create_directories(parent, ec);
        if (ec || std::filesystem::exists(path, ec) || ec ||
            !std::filesystem::create_directory(path, ec) || ec) return nullptr;

        auto database = std::make_unique<KVStore>(KVStoreOptions{.path = path});
        const auto encoded = EncodeBootstrapNetworkBinding(binding);
        if (!encoded) return nullptr;
        database->Write(std::string{BINDING_KEY}, *encoded, true);
        return std::unique_ptr<BootstrapBindingStore>{
            new BootstrapBindingStore{std::move(database), binding}};
    } catch (...) {
        return nullptr;
    }
}

std::unique_ptr<BootstrapBindingStore> BootstrapBindingStore::Open(const std::filesystem::path& path)
{
    try {
        std::error_code ec;
        if (!std::filesystem::is_directory(path, ec) || ec) return nullptr;
        auto database = std::make_unique<KVStore>(KVStoreOptions{.path = path});
        std::vector<unsigned char> encoded;
        if (!database->Read(std::string{BINDING_KEY}, encoded)) return nullptr;
        auto binding = DecodeBootstrapNetworkBinding(encoded);
        if (!binding) return nullptr;
        return std::unique_ptr<BootstrapBindingStore>{
            new BootstrapBindingStore{std::move(database), std::move(*binding)}};
    } catch (...) {
        return nullptr;
    }
}

BootstrapNetworkBinding BootstrapBindingStore::Current() const
{
    std::lock_guard lock{m_mutex};
    return m_binding;
}

BootstrapBindingAcceptStatus BootstrapBindingStore::AcceptNext(const BootstrapNetworkBinding& binding)
{
    std::lock_guard lock{m_mutex};
    if (!VerifyBootstrapNetworkBinding(binding)) return BootstrapBindingAcceptStatus::INVALID_BINDING;
    const auto current_key = AuthorityKey(m_binding);
    const auto candidate_key = AuthorityKey(binding);
    if (!current_key || !candidate_key) return BootstrapBindingAcceptStatus::INVALID_BINDING;
    if (*current_key != *candidate_key) return BootstrapBindingAcceptStatus::AUTHORITY_CHANGED;

    if (binding.generation < m_binding.generation) return BootstrapBindingAcceptStatus::GENERATION_ROLLBACK;
    if (binding.generation == m_binding.generation) {
        const auto current_bytes = EncodeBootstrapNetworkBinding(m_binding);
        const auto candidate_bytes = EncodeBootstrapNetworkBinding(binding);
        if (!current_bytes || !candidate_bytes) return BootstrapBindingAcceptStatus::INVALID_BINDING;
        return *current_bytes == *candidate_bytes ? BootstrapBindingAcceptStatus::UNCHANGED :
            BootstrapBindingAcceptStatus::GENERATION_CONFLICT;
    }

    const auto encoded = EncodeBootstrapNetworkBinding(binding);
    if (!encoded) return BootstrapBindingAcceptStatus::INVALID_BINDING;
    try {
        m_database->Write(std::string{BINDING_KEY}, *encoded, true);
    } catch (...) {
        return BootstrapBindingAcceptStatus::STORAGE_ERROR;
    }
    m_binding = binding;
    return BootstrapBindingAcceptStatus::ACCEPTED;
}

BootstrapBindingAcceptStatus BootstrapBindingStore::AcceptReplacement(
    const BootstrapNetworkReplacement& replacement)
{
    std::lock_guard lock{m_mutex};
    if (!VerifyBootstrapNetworkReplacement(m_binding, replacement)) {
        if (replacement.new_binding.generation <= m_binding.generation)
            return replacement.new_binding.generation < m_binding.generation ?
                BootstrapBindingAcceptStatus::GENERATION_ROLLBACK :
                BootstrapBindingAcceptStatus::GENERATION_CONFLICT;
        return BootstrapBindingAcceptStatus::INVALID_BINDING;
    }
    const auto encoded = EncodeBootstrapNetworkBinding(replacement.new_binding);
    if (!encoded) return BootstrapBindingAcceptStatus::INVALID_BINDING;
    try {
        m_database->Write(std::string{BINDING_KEY}, *encoded, true);
    } catch (...) {
        return BootstrapBindingAcceptStatus::STORAGE_ERROR;
    }
    m_binding = replacement.new_binding;
    return BootstrapBindingAcceptStatus::ACCEPTED;
}

} // namespace cybou
