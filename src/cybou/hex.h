// Copyright (c) 2026 CYBOU contributors
/// \file
/// \brief Русский публичный API строгого hex-кодирования для пользовательского ввода и вывода.
#ifndef CYBOU_HEX_H
#define CYBOU_HEX_H
#include <cybou/hash256.h>
#include <span>
namespace cybou {
/// \brief Разбирает пользовательский 64-символьный hex Hash256 без префиксов.
std::optional<Hash256> ParseHash256UserHex(std::string_view input);
/// \brief Кодирует произвольный буфер в строчный hex без разделителей.
std::string HexEncode(std::span<const unsigned char> bytes);
}
#endif
