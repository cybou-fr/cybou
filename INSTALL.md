# Building and Installing CYBOU

This guide provides entrypoint instructions for building and running **CYBOU** from source.

CYBOU officially supports:
- **Windows (MSVC)**: Primary platform for the single-process `cybou.exe` desktop application.
- **Linux (x86_64 / aarch64)**: Infrastructure nodes, active validators, and containerized deployments.

---

## Quick Start by Platform

### Windows (Recommended for Desktop Users and Developers)

Prerequisites:
- Windows 10/11
- Visual Studio 2022/2026 with the "Desktop development with C++" workload
- CMake (bundled with Visual Studio)
- Python 3

Full instructions:
👉 **[Windows MSVC Build Guide](doc/build-windows-msvc.md)**

```powershell
# Open Developer PowerShell for VS
cmake -B build -S .
cmake --build build --config Release
```

The resulting executable is `build/src/qt/Release/cybou.exe`.

---

### Linux / Unix (Infrastructure and Validators)

Prerequisites:
- Ubuntu 22.04 LTS+, Debian 12+, or Fedora
- C++20 compliant compiler (GCC 11+ or Clang 14+)
- CMake 3.22+
- Boost, libevent, and Qt6 (for GUI)

Full instructions:
👉 **[Linux / Unix Build Guide](doc/build-unix.md)**

```bash
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

---

## Dependencies

For complete details on third-party packages, system libraries, and version requirements, consult:
👉 **[Dependencies Reference](doc/dependencies.md)**

---

## Testing the Build

To run the unit test suite after compilation:

```powershell
ctest --test-dir build --output-on-failure
```
