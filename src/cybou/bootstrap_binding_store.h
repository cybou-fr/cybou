// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_BOOTSTRAP_BINDING_STORE_H
#define CYBOU_BOOTSTRAP_BINDING_STORE_H

#include <cybou/bootstrap_binding.h>
#include <cybou/kv_store.h>

#include <filesystem>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string_view>

namespace cybou {

enum class BootstrapBindingAcceptStatus : uint8_t {
    ACCEPTED,
    UNCHANGED,
    INVALID_BINDING,
    GENERATION_ROLLBACK,
    GENERATION_CONFLICT,
    AUTHORITY_CHANGED,
    STORAGE_ERROR,
};

/** Durable client pin for a trusted binding and its accepted generation. */
class BootstrapBindingStore final {
public:
    /** Create a client pin from an explicitly trusted first binding. */
    static std::unique_ptr<BootstrapBindingStore> TrustInitial(
        const std::filesystem::path& path, const BootstrapNetworkBinding& binding);
    static std::unique_ptr<BootstrapBindingStore> Open(const std::filesystem::path& path);

    BootstrapBindingStore(const BootstrapBindingStore&) = delete;
    BootstrapBindingStore& operator=(const BootstrapBindingStore&) = delete;

    BootstrapNetworkBinding Current() const;
    BootstrapBindingAcceptStatus AcceptNext(const BootstrapNetworkBinding& binding);

private:
    BootstrapBindingStore(std::unique_ptr<KVStore> database, BootstrapNetworkBinding binding);

    std::unique_ptr<KVStore> m_database;
    mutable std::mutex m_mutex;
    BootstrapNetworkBinding m_binding;
};

} // namespace cybou

#endif // CYBOU_BOOTSTRAP_BINDING_STORE_H
