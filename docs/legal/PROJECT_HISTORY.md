# Project history

CYBOU originated from an imported Bitcoin Core source tree (Bitcoin Core
31.1.0, CYBOU import commit `6de2974`). The active CYBOU protocol, runtime,
networking, finality, identity, storage, desktop and build system are
independent CYBOU components; the earlier source remains available through Git
history.

On 2026-10-06 the tracked tree was compared with Bitcoin Core v31.1 (tag
commit `9be056a`). Apart from the vendored LevelDB and crc32c libraries, which
keep their own licenses, only `COPYING` is identical: it keeps the original MIT
text and its copyright notices as the record of that origin. No other file is
more than 64% similar; the closest, `.editorconfig` and `vcpkg.json`, share
only their standard file formats.
