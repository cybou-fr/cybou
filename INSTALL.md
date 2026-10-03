# Building CYBOU from source

CYBOU is experimental software. The single `cybou` executable (Qt desktop plus headless node commands) is a DEV integration target. The PQ identity and state cutover is in progress; see [implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md) before using a build as a network node.

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
cmake --build build_cybou_qt_mingw --target cybou cybou-core-test -j 4
& build_cybou_qt_mingw/bin/cybou-core-test.exe --log_level=error
```

`build_cybou_qt_mingw/bin/cybou.exe` is the desktop when started without a command and the headless node with one, for example `cybou.exe network info --network devnet` or `cybou.exe node run --network devnet ...`. A `BUILD_GUI=OFF` build produces a headless-only `cybou`. `cybou-loadgen` is built only with `BUILD_TESTS=ON`.

## Linux and other platforms

The project has CMake/vcpkg CI builds for the native core and desktop; see [the core workflow](.github/workflows/cybou-core.yml) and [the desktop workflow](.github/workflows/cybou-desktop.yml). Desktop CI covers Windows and Ubuntu 24.04; the Linux Qt smoke test runs under Xvfb.

## DEV node

The DEV VPS (`51.255.46.58`, TCP port 29461; SSH on port 22) still runs a retired prototype bootstrap service built from an older commit. This repository no longer builds that executable; the planned replacement is an ordinary headless `cybou` node on the compiled DEVNET (`cybou node run --network devnet ... --tls-certificate FILE --tls-key FILE`). The DEVNET bootstrap locator and its TLS SPKI SHA-256 pin are compiled in [`src/cybou/official_networks.cpp`](src/cybou/official_networks.cpp). Nodes and the desktop dial that locator automatically and check its compiled pin, so the VPS must present the matching certificate. This deployment is a development prototype, not the target architecture described in [`docs/cybou/04_NETWORK_LIFECYCLE.md`](docs/cybou/04_NETWORK_LIFECYCLE.md). Never use development keys or balances as production assets.

## Tests

`cybou-core-test` contains native CYBOU protocol tests for state, Identity, PoA, RootPublication, and transport behavior. `ctest --test-dir <build-dir> --output-on-failure` runs the configured broader suite. The Qt shell tests are separate and require a GUI-capable environment.
