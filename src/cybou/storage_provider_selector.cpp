// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0

/// \file
/// \brief Uniform randomized ordering by payout identity and StorageId.

#include <cybou/storage_service_internal.h>
#include <openssl/rand.h>
#include <limits>

namespace cybou {

namespace {
/// Равномерное CSPRNG-перемешивание; false означает отказ системного RNG.
template <typename T>
bool ShuffleItems(std::vector<T>& items)
{
    for (std::size_t i = items.size(); i > 1; --i) {
        const std::uint64_t bound = i;
        const std::uint64_t limit = std::numeric_limits<std::uint64_t>::max() -
            std::numeric_limits<std::uint64_t>::max() % bound;
        std::uint64_t draw{0};
        do {
            if (RAND_bytes(reinterpret_cast<unsigned char*>(&draw), sizeof(draw)) != 1) return false;
        } while (draw >= limit);
        std::swap(items[i - 1], items[draw % bound]);
    }
    return true;
}

} // namespace

/// Экономическая идентичность provider'а: payout-аккаунт, а без binding — сам StorageId (DEC-280).
std::array<unsigned char, 32> StorageService::ProviderSelector::EconomicIdentity(const StorageEndpoint& provider)
{
    return provider.payout_account.value_or(provider.storage_id);
}

bool StorageService::ProviderSelector::OrderProviders(std::vector<StorageEndpoint>& providers)
{
    bool shuffled = ShuffleItems(providers);
    std::map<std::array<unsigned char, 32>, std::vector<StorageEndpoint>> groups;
    std::vector<std::array<unsigned char, 32>> group_order;
    for (auto& provider : providers) {
        auto& group = groups[EconomicIdentity(provider)];
        if (group.empty()) group_order.push_back(EconomicIdentity(provider));
        group.push_back(std::move(provider));
    }
    shuffled = shuffled && ShuffleItems(group_order);
    providers.clear();
    for (const auto& key : group_order) {
        for (auto& provider : groups[key]) providers.push_back(std::move(provider));
    }
    return shuffled;
}

bool StorageService::ProviderSelector::Shuffle(std::vector<StorageEndpoint>& providers)
{
    return ShuffleItems(providers);
}

} // namespace cybou
