# CI and local verification

The maintained CI pipelines live in [`.github/workflows`](../.github/workflows).
The core workflow builds CYBOU with CMake and runs the C++ protocol tests,
operator CLI acceptance, desktop controller checks, storage smoke, and storage
soak. The desktop workflow covers the Qt/vcpkg build.

For a local core run, configure with `-DBUILD_GUI=OFF -DBUILD_TESTS=ON`, build
`cybou-core-test` and `cybou-node`, then run `ctest --test-dir <build-dir> --output-on-failure`
and `python test/cybou_operator_cli.py <build-dir>/bin/cybou-node`.
