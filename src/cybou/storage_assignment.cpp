// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
#include <cybou/storage_assignment.h>
#include <cybou/binary_codec.h>
#include <cybou/crypto/sha256.h>
#include <cybou/hash256.h>
#include <algorithm>
#include <limits>
#include <map>

namespace cybou {
namespace {
bool Empty(const StorageAssignmentId& id) { return std::all_of(id.begin(), id.end(), [](auto b) { return b == 0; }); }
StorageAssignmentId Digest(std::string_view domain, std::span<const unsigned char> bytes)
{
    StorageAssignmentId result{};
    if (!crypto::ComputeSha256({crypto::Sha256Bytes(domain), bytes}, result.data())) {
        throw std::runtime_error{"assignment hash failed"};
    }
    return result;
}
void Context(BinaryWriter& out, const StorageAssignmentContext& c)
{
    out.Fixed(c.network_binding); out.Fixed(c.publication); out.Fixed(c.chunk);
    out.Fixed(c.finalized_seed); out.Fixed(c.payer);
    out.U64(c.epoch); out.U64(c.term_start); out.U64(c.term_end); out.U8(c.replicas);
}
void Providers(BinaryWriter& out, const std::vector<StorageAssignmentProvider>& providers)
{
    out.U32(static_cast<std::uint32_t>(providers.size()));
    for (const auto& p : providers) { out.Fixed(p.storage_id); out.Fixed(p.payout_account); }
}
std::vector<unsigned char> Encode(const StorageAssignmentPlan& plan, bool selected)
{
    BinaryWriter out;
    Context(out, plan.context); Providers(out, plan.eligible);
    if (selected) Providers(out, plan.selected);
    return out.Take();
}
std::string Key(const StorageAssignmentContext& c)
{
    BinaryWriter out;
    out.Fixed(c.network_binding); out.Fixed(c.publication); out.Fixed(c.chunk);
    out.U64(c.epoch); out.U64(c.term_start); out.U64(c.term_end);
    const auto digest = Digest("CYBOU/STORAGE-ASSIGNMENT-KEY", out.Take());
    return "storage/assignment/" + Hash256{std::span<const unsigned char, 32>{digest}}.GetHex();
}
template <typename T> void Shuffle(std::vector<T>& items, const StorageAssignmentId& input, std::uint64_t& counter)
{
    for (std::size_t size = items.size(); size > 1; --size) {
        const std::uint64_t bound = size;
        const auto limit = std::numeric_limits<std::uint64_t>::max() -
            std::numeric_limits<std::uint64_t>::max() % bound;
        std::uint64_t draw;
        do {
            if (counter == std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error{"assignment draw overflow"};
            BinaryWriter out; out.Fixed(input); out.U64(counter++);
            const auto hash = Digest("CYBOU/STORAGE-ASSIGNMENT-DRAW", out.Take());
            draw = ReadLittleEndian<std::uint64_t>(hash);
        } while (draw >= limit);
        std::swap(items[size - 1], items[draw % bound]);
    }
}
}

std::optional<StorageAssignmentPlan> PrepareStorageAssignment(StorageAssignmentContext context,
    std::vector<StorageAssignmentProvider> eligible)
{
    if (Empty(context.network_binding) || Empty(context.publication) || Empty(context.chunk) ||
        Empty(context.finalized_seed) || Empty(context.payer) || context.term_end <= context.term_start ||
        context.replicas == 0 || context.replicas > 2 || eligible.empty() ||
        eligible.size() > MAX_STORAGE_ASSIGNMENT_CANDIDATES) return std::nullopt;
    std::sort(eligible.begin(), eligible.end());
    eligible.erase(std::unique(eligible.begin(), eligible.end()), eligible.end());
    std::map<StorageAssignmentId, std::vector<StorageAssignmentProvider>> groups;
    for (std::size_t i{0}; i < eligible.size(); ++i) {
        const auto& p = eligible[i];
        if (Empty(p.storage_id) || Empty(p.payout_account) || p.payout_account == context.payer ||
            (i && eligible[i - 1].storage_id == p.storage_id)) return std::nullopt;
        groups[p.payout_account].push_back(p);
    }
    if (groups.size() < context.replicas) return std::nullopt;
    StorageAssignmentPlan plan{.context = context, .eligible = std::move(eligible)};
    const auto input = Digest("CYBOU/STORAGE-ASSIGNMENT-INPUT", Encode(plan, false));
    std::vector<StorageAssignmentId> accounts;
    for (const auto& [account, _] : groups) accounts.push_back(account);
    std::uint64_t counter{0};
    Shuffle(accounts, input, counter); // One chance per payout identity, not per endpoint/key.
    for (std::size_t i{0}; i < context.replicas; ++i) {
        auto& providers = groups.at(accounts[i]);
        Shuffle(providers, input, counter);
        plan.selected.push_back(providers.front());
    }
    plan.commitment = Digest("CYBOU/STORAGE-ASSIGNMENT-COMMITMENT", Encode(plan, true));
    return plan;
}

bool FreezeStorageAssignment(PrivateApplicationStore& db, const StorageAssignmentPlan& plan)
{
    const auto expected = PrepareStorageAssignment(plan.context, plan.eligible);
    if (!expected || *expected != plan) return false;
    PrivateApplicationStore::Batch batch{db};
    const auto key = Key(plan.context);
    const auto encoded = Encode(plan, true);
    if (const auto prior = db.Get(key)) return *prior == encoded;
    if (db.Has(key)) return false; // Existing unreadable records are never overwritten.
    return db.Put(key, encoded) && batch.Commit();
}

std::optional<StorageAssignmentPlan> LoadStorageAssignment(PrivateApplicationStore& db,
    const StorageAssignmentContext& context)
{
    const auto bytes = db.Get(Key(context));
    if (!bytes) return std::nullopt;
    try {
        BinaryReader in{*bytes};
        StorageAssignmentContext c;
        c.network_binding = in.Fixed<StorageAssignmentId>(); c.publication = in.Fixed<StorageAssignmentId>();
        c.chunk = in.Fixed<StorageAssignmentId>(); c.finalized_seed = in.Fixed<StorageAssignmentId>();
        c.payer = in.Fixed<StorageAssignmentId>(); c.epoch = in.U64(); c.term_start = in.U64();
        c.term_end = in.U64(); c.replicas = in.U8();
        if (c != context) return std::nullopt;
        const auto read = [&in](std::size_t maximum) {
            const auto count = in.U32();
            if (count > maximum) throw std::length_error{"assignment provider limit"};
            std::vector<StorageAssignmentProvider> result;
            for (std::uint32_t i{0}; i < count; ++i) result.push_back(
                {in.Fixed<StorageAssignmentId>(), in.Fixed<StorageAssignmentId>()});
            return result;
        };
        auto eligible = read(MAX_STORAGE_ASSIGNMENT_CANDIDATES);
        const auto selected = read(2);
        in.Finish();
        auto plan = PrepareStorageAssignment(c, std::move(eligible));
        if (!plan || plan->selected != selected || Encode(*plan, true) != *bytes) return std::nullopt;
        return plan;
    } catch (const std::invalid_argument&) { return std::nullopt;
    } catch (const std::length_error&) { return std::nullopt; }
}
}
