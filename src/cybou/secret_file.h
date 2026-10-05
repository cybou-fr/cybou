// Copyright (c) 2026 Stanislav Saveliev
// SPDX-License-Identifier: Apache-2.0
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
/// \param path Путь к файлу секрета.
/// \return `FILE*` в бинарном append-режиме, либо `nullptr`, если файл не приватный, не обычный или открыть его безопасно нельзя.
/// \post На Windows и POSIX симлинки/rewparse-point не допускаются.
std::FILE* OpenPrivateAppendFile(const std::filesystem::path&);
/// \brief Создает новый приватный файл и целиком записывает в него секретные байты.
/// \param path Путь для создания; существующий файл не перезаписывается.
/// \param bytes Секретные байты длиной от 1 до 1 MiB.
/// \return `true` только если файл создан с приватными правами и все байты durably записаны.
/// \post При любой частичной ошибке функция старается удалить недописанный файл.
bool CreateSecretFile(const std::filesystem::path&, std::span<const unsigned char>);
/// \brief Читает приватный файл целиком, если его размер и права доступа допустимы.
/// \param path Путь к уже существующему файлу.
/// \param max_bytes Верхняя граница допустимого размера, от 1 до 1 MiB.
/// \return Содержимое файла, либо `std::nullopt`, если размер, тип файла или права доступа не проходят fail-closed проверку.
std::optional<std::vector<unsigned char>> ReadSecretFile(const std::filesystem::path&, size_t max_bytes);
}
#endif
