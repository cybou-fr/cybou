# Developer tools

- `circular-dependencies.py` reports include-based source dependency cycles.
  Run it from `src/`, passing the source files to inspect.
- `clang-format-diff.py` formats changed lines from a unified diff; see
  `--help` for options.
- `split-debug.sh.in` is configured by the CMake maintenance module on Linux.

Bitcoin-specific library dependency checks and include-what-you-use mappings
were removed after their targets and CI jobs were retired.
