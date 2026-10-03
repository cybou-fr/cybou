# CI and local verification

The maintained CI pipelines live in [`.github/workflows`](../.github/workflows).
The core workflow builds CYBOU with CMake, runs the C++ component/protocol tests
and checks the official DEVNET CLI without starting another network. The desktop
workflow covers the Qt/vcpkg build.

For a local core run, configure with `-DBUILD_GUI=OFF -DBUILD_TESTS=ON`, build
`cybou-core-test` and `cybou`, then run
`ctest --test-dir <build-dir> --output-on-failure`.

From a France-admitted development machine, run
`python test/cybou_operator_cli.py <build-dir>/bin/cybou` against existing DEVNET.
This check uses its compiled locator, creates no network or signing material and
never enables PoA. Storage finality/fault checks use the existing DEVNET signer
and require the operator to unlock it. Public CI runners have no admission bypass.
