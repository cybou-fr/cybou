// Copyright (c) 2018-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/cybouapplication.h>

#include <compat/compat.h>
#include <cybou/cli/cybou_cli.h>

#ifdef WIN32
#include <windows.h>

#include <cstdio>
#endif

MAIN_FUNCTION
{
    // One executable: a headless command runs the node/operator CLI, otherwise the desktop.
    if (argc > 1 && cybou::cli::IsCommand(argv[1])) {
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
