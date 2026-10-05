// Copyright (c) 2026 Stanislav SAVELIEV
// SPDX-License-Identifier: Apache-2.0

// Headless build of the single `cybou` executable (BUILD_GUI=OFF).
#include <cybou/cli/cybou_cli.h>

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "cybou: this build has no desktop; use --help for headless commands\n";
        return 1;
    }
    return cybou::cli::Run(argc, argv);
}
