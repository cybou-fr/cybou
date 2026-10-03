// Copyright (c) 2026 Stanislav Saveliev
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русский публичный API безопасного чтения и записи приватных файлов.
#ifndef CYBOU_SECRET_FILE_H
#define CYBOU_SECRET_FILE_H
#include <filesystem>
#include <optional>
#include <span>
#include <vector>
#include <cstdio>
namespace cybou {
/// \brief Открывает существующий или новый приватный файл для append с проверкой его прав.
std::FILE* OpenPrivateAppendFile(const std::filesystem::path&);
/// \brief Создает новый приватный файл и целиком записывает в него секретные байты.
bool CreateSecretFile(const std::filesystem::path&, std::span<const unsigned char>);
/// \brief Читает приватный файл целиком, если его размер и права доступа допустимы.
std::optional<std::vector<unsigned char>> ReadSecretFile(const std::filesystem::path&, size_t max_bytes);
}
#endif
