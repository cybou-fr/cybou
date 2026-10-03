// Copyright (c) 2026 Stanislav SAVELIEV
// Distributed under the MIT software license, see the accompanying file COPYING.

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
