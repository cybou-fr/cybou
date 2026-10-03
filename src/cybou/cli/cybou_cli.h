// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

#ifndef CYBOU_CLI_CYBOU_CLI_H
#define CYBOU_CLI_CYBOU_CLI_H

#include <string_view>

namespace cybou::cli {

/** True when argv[1] selects the headless CLI of the single `cybou` executable. */
bool IsCommand(std::string_view first_argument);

/** Runs one headless command (node roles, network, operation, doctor, storage). */
int Run(int argc, char* argv[]);

} // namespace cybou::cli

#endif // CYBOU_CLI_CYBOU_CLI_H
