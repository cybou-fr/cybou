// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
#ifndef CYBOU_SECRET_FILE_H
#define CYBOU_SECRET_FILE_H
#include <filesystem>
#include <optional>
#include <span>
#include <vector>
#include <cstdio>
namespace cybou {
std::FILE* OpenPrivateAppendFile(const std::filesystem::path&);
bool CreateSecretFile(const std::filesystem::path&, std::span<const unsigned char>);
std::optional<std::vector<unsigned char>> ReadSecretFile(const std::filesystem::path&, size_t max_bytes);
}
#endif
