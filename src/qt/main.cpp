// Copyright (c) 2026 CYBOU contributors
// SPDX-License-Identifier: Apache-2.0

#include <qt/cybouapplication.h>

#include <cybou/cli/cybou_cli.h>

#ifdef WIN32
#include <windows.h>

#include <cstdio>
#endif

int main(int argc, char* argv[])
{
    // One executable: a headless command runs the node/operator CLI, otherwise the desktop.
    if (argc > 1 && (cybou::cli::IsCommand(argv[1]) || argv[1][0] != '-')) {
#ifdef WIN32
        // The desktop is a GUI-subsystem binary; reuse the caller's console for CLI output.
        if (AttachConsole(ATTACH_PARENT_PROCESS)) {
            std::freopen("CONOUT$", "w", stdout);
            std::freopen("CONOUT$", "w", stderr);
        }
#endif
        return cybou::cli::Run(argc, argv);
    }
    return CybouQtMain(argc, argv);
}
