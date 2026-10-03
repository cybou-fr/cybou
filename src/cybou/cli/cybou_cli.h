// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русская точка входа headless CLI единственного исполняемого файла `cybou`.

#ifndef CYBOU_CLI_CYBOU_CLI_H
#define CYBOU_CLI_CYBOU_CLI_H

#include <string_view>

namespace cybou::cli {

/// \brief Возвращает true, если первый аргумент выбирает headless CLI.
bool IsCommand(std::string_view first_argument);

/// \brief Выполняет одну headless-команду (`node`, `network`, `operation`, `doctor`, `storage`).
int Run(int argc, char* argv[]);

} // namespace cybou::cli

#endif // CYBOU_CLI_CYBOU_CLI_H
