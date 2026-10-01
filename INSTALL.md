# Building CYBOU from source

CYBOU is experimental software. The native `cybou-node` and Qt desktop are DEV integration targets. The PQ identity and state cutover is in progress; see [implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md) before using a build as a network node.

## Requirements

- CMake 3.22 or newer and a C++20 compiler.
- OpenSSL **3.5 or newer** for the hybrid Ed25519 + ML-DSA-65 PoA signer.
- Boost, libevent, and LevelDB dependencies as configured by CMake/vcpkg.
- Qt 6 for the optional desktop executable `cybou` (`cybou.exe` on Windows).

The repository pins third-party dependencies in [`vcpkg.json`](vcpkg.json). A generic system build will fail configuration if its OpenSSL version is older than 3.5.

## Windows desktop and core

The currently verified local setup is **Qt MinGW + vcpkg**, documented step by step in [the Windows build procedure](docs/cybou/71_WINDOWS_MINGW_BUILD.md). It configures `build_cybou_qt_mingw` with `BUILD_GUI=ON` and `BUILD_TESTS=ON`.

After following that procedure, build the native targets:

```powershell
cmake --build build_cybou_qt_mingw --target cybou cybou-node cybou-core-test -j 4
& build_cybou_qt_mingw/bin/cybou-core-test.exe --log_level=error
```

The desktop executable is `build_cybou_qt_mingw/bin/cybou.exe`; the standalone DEV process is `build_cybou_qt_mingw/bin/cybou-node.exe`.

## Linux and other platforms

The project has a CMake/vcpkg CI build for the native core and `cybou-node`; see [the core workflow](.github/workflows/cybou-core.yml) for the exact configure, test, and smoke-test commands. Linux desktop builds need platform-specific verification.

## DEV node

The current DEV installation is an operational legacy testnet topology (`cybou-node.service` PoA finalizer on port 29461 and independent storage providers on ports 29471/29481). It is not the target architecture described in [`docs/cybou/04_NETWORK_BOOTSTRAP_AND_GENESIS.md`](docs/cybou/04_NETWORK_BOOTSTRAP_AND_GENESIS.md). Keep the current DEV testnet operational for routine development until specified acceptance tests and the planned new-genesis DEV cutover are complete. Do not reset DEV state or replace the PoA key during routine development. Never use development keys or balances as production assets.

## Tests

`cybou-core-test` contains native CYBOU protocol tests for state, Identity, PoA, RootPublication, and transport behavior. `ctest --test-dir <build-dir> --output-on-failure` runs the configured broader suite. The Qt shell tests are separate and require a GUI-capable environment.
