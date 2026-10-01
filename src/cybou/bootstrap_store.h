// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_STORE_H
#define CYBOU_BOOTSTRAP_STORE_H

#include <cybou/bootstrap_binding.h>
#include <cybou/kv_store.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace cybou {

enum class BootstrapStoreState : uint8_t { EMPTY = 0, BOUND = 1 };
enum class BootstrapClaimStatus : uint8_t {
    CLAIMED,
    ALREADY_BOUND,
    INVALID_ACTIVATION_CODE,
    INVALID_BINDING,
    STORAGE_ERROR,
};
enum class BootstrapReplacementStatus : uint8_t {
    REPLACED,
    NOT_BOUND,
    INVALID_REPLACEMENT,
    ARCHIVE_CONFLICT,
    STORAGE_ERROR,
};

class BootstrapStore;

struct BootstrapProvision {
    std::unique_ptr<BootstrapStore> store;
    std::string activation_code;
};

/** Durable single-network bootstrap state. The activation code is never stored. */
class BootstrapStore final {
public:
    static std::optional<BootstrapProvision> Provision(const std::filesystem::path& path);
    static std::unique_ptr<BootstrapStore> Open(const std::filesystem::path& path);
    static std::optional<std::string> GenerateActivationCode();

    BootstrapStore(const BootstrapStore&) = delete;
    BootstrapStore& operator=(const BootstrapStore&) = delete;

    BootstrapStoreState State() const;
    std::optional<BootstrapNetworkBinding> CurrentBinding() const;
    std::optional<BootstrapNetworkBinding> ArchivedBinding(uint64_t generation) const;
    BootstrapClaimStatus ClaimInitialNetwork(std::string_view activation_code,
        const BootstrapNetworkBinding& binding);
    BootstrapReplacementStatus ReplaceNetwork(const BootstrapNetworkReplacement& replacement);

private:
    BootstrapStore(std::unique_ptr<KVStore> database, BootstrapStoreState state,
        std::array<unsigned char, 32> activation_code_hash,
        std::optional<BootstrapNetworkBinding> binding);

    std::unique_ptr<KVStore> m_database;
    mutable std::mutex m_mutex;
    BootstrapStoreState m_state;
    std::array<unsigned char, 32> m_activation_code_hash{};
    std::optional<BootstrapNetworkBinding> m_binding;
};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_STORE_H
