// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.
/// \file
/// \brief Русская точка входа headless CLI единственного исполняемого файла `cybou`.

#ifndef CYBOU_CLI_CYBOU_CLI_H
#define CYBOU_CLI_CYBOU_CLI_H

#include <string_view>

namespace cybou::cli {

/// \brief Возвращает true, если первый аргумент выбирает headless CLI.
/// \param first_argument Первый пользовательский аргумент после имени процесса.
/// \return `true` для известных headless-команд; `false` означает, что управление может перейти desktop-режиму.
bool IsCommand(std::string_view first_argument);

/// \brief Выполняет одну headless-команду (`node`, `network`, `operation`, `doctor`, `storage`).
/// \param argc Стандартное количество аргументов `main`.
/// \param argv Стандартный массив аргументов `main`.
/// \return Процессный код завершения выбранной CLI-команды.
/// \pre `argv` содержит как минимум `argc` элементов.
int Run(int argc, char* argv[]);

} // namespace cybou::cli

#endif // CYBOU_CLI_CYBOU_CLI_H
