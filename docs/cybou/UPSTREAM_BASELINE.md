# Upstream baseline record

## Identified release

The imported source tree identifies itself as **Bitcoin Core 31.1.0**:

- `CLIENT_VERSION_MAJOR=31`, `CLIENT_VERSION_MINOR=1` in the root build file;
- the CYBOU rename inventory independently names Bitcoin Core v31.1;
- inherited copyright and MIT license notices are retained.

## Provenance limitation

The local Git history begins with CYBOU import commit `6de2974`; it does not
contain authenticated Bitcoin Core ancestry. Consequently, the exact upstream
commit, tag signature, source URL, and archive checksum cannot be recovered from
the repository history alone.

Gate A (exact upstream baseline) therefore remains **open**. Before a release or
trusted baseline tag, the operator must reconcile this tree with an authenticated
Bitcoin Core v31.1 source artifact and record:

1. exact commit hash and signed tag verification;
2. source URL and archive SHA-256;
3. clean-tree comparison exceptions;
4. compiler, CMake, vcpkg, and Qt versions;
5. clean build and upstream test results.

This record is evidence of the locally identifiable version, not a claim that
the unavailable upstream commit has been cryptographically verified.
