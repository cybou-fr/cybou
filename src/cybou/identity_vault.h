// Copyright (c) 2026 The CYBOU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/license/mit/.

#ifndef CYBOU_IDENTITY_VAULT_H
#define CYBOU_IDENTITY_VAULT_H

#include <optional>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>

namespace cybou {

// Portable CYBV2 cryptographic envelope. File persistence and payload schema
// are separate; callers must not broadcast before durable save/reopen.
std::optional<std::vector<unsigned char>> SealIdentityVault(
    std::string_view password, std::span<const unsigned char> payload);
std::optional<std::vector<unsigned char>> OpenIdentityVault(
    std::string_view password, std::span<const unsigned char> envelope);

// Creates a new vault without replacing an existing identity. Returns true
// only after a synced write, atomic publication, and authenticated reopen.
bool SaveNewIdentityVault(const std::filesystem::path& path,
    std::string_view password, std::span<const unsigned char> payload);
std::optional<std::vector<unsigned char>> LoadIdentityVault(
    const std::filesystem::path& path, std::string_view password);

} // namespace cybou
#endif
