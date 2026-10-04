# Diagnostic logging, retention and support export

Reviewed: 2026-10-04, code baseline `20ab3fd`. Governing baseline:
[`SECURITY_GOVERNANCE.md`](SECURITY_GOVERNANCE.md). Inventory:
[`DATA_PROCESSING_INVENTORY.md`](DATA_PROCESSING_INVENTORY.md).
This document establishes acceptance requirements and a proposed implementation
contract; it does not claim rotation/export features are implemented or change
live service configuration.

## Current code and deployment evidence

| Surface | Reviewed behavior | Boundary |
|---|---|---|
| Headless JSON events | `cli/cybou_cli.cpp::StartEvents` creates EventWriter only with `--event-log`; `--event-log-mode` defaults to minimal | Opt-in file logging. Minimal filtering applies to this writer, not every CLI output |
| Event fields | `event_record.cpp` allowlists names and bounds strings; minimal omits account_id, nonce, peer, storage_id, chunk_id and operation_id | Detailed keeps these fields. Allowlisted names do not guarantee that every string value is non-sensitive |
| Append file | `secret_file.cpp::OpenPrivateAppendFile` checks private regular-file access and rejects symlink/reparse inputs; POSIX creation uses 0600; Windows checks owner/SYSTEM access | Protect parent directories and backups too. File access controls are not anonymization or a retention policy |
| Durability/rotation | Writer appends a line, calls fflush, and holds its file descriptor until destruction | No writer-level age/size rollover, pruning or reopen signal; fflush is not filesystem power-loss durability certification |
| stdout/stderr | CLI operation/peer commands print identifiers/endpoints; node service can report event-log failure to stderr | Journal/terminal/service collection is an independent data path; minimal JSON mode does not redact it |
| Desktop | No EventWriter construction or event-log setting was found in reviewed `src/qt` sources | This does not inventory Qt/framework crash logs, OS logging, exported files or all diagnostics views |
| `storage status` | CLI reads up to the last 64 KiB of a supplied event file and outputs a complete matching node_status line | It is a status reader, not a redacting support exporter; substring matching is not a privacy sanitizer |
| DEV service | Repository instructions identify the service and state location | This pass did not inspect live systemd/journald or backup settings. No live rotation, expiry or minimal-mode setting is asserted |

## Requirements for ordinary diagnostic logs

Minimal is the normal JSON-event mode. Detailed is a deliberate diagnostic
choice with a recorded purpose, operator access boundary, end condition and
review of exports. Both modes must exclude vault contents, mnemonic words,
signing keys, passwords, plaintext messages and filenames. The
allowlist rejects fields named for secrets, but does not inspect arbitrary
string contents; call-site and export validation remain necessary.

Before release, each installation profile must specify its diagnostic purpose,
maximum retained age, byte budget, segment count, authorized readers and backup
expiry. No universal legal retention duration is chosen here. Apply
[CNIL retention guidance](https://www.cnil.fr/fr/passer-laction/les-durees-de-conservation-des-donnees)
and [logging guidance](https://www.cnil.fr/fr/securite-tracer-les-operations)
to the assessed purpose, obligations and exceptions; record the rationale.
Size bounds prevent disk exhaustion but do not replace an age limit.

Native rollover must serialize with writes: close/flush, move the active file
within a protected directory, reopen a private regular file, and prune only
explicitly owned diagnostic segments. Reject symlinks/reparse points and
untrusted paths. Preserve complete records or explicitly report truncation/loss;
bound segment count and work per maintenance pass. Reopen/rename/prune failures
must be observable and tested without deleting unrelated files.

External rename alone is not a supported active-writer rotation mechanism:
POSIX continues writing through the old descriptor; Windows opens with
FILE_SHARE_READ and no delete sharing. Copy/truncate has write-loss races and
is not the proposed default. Any interim close/reopen procedure needs a reviewed
maintenance plan, especially for a signer process; none is executed by this work.

## Proposed support-export contract

Support export must be an explicit local user action producing a new reviewed
artifact; no automatic upload, messaging or collection of private directories.
It must parse bounded JSON records, validate exact field names and types,
reject or count malformed/truncated/oversized records, and project to a positive
export schema rather than copying input text. Missing evidence is reported as
missing, not synthesized as Verified.

Initial export candidates are event kind, relative sequence/time, numeric
height, counters and validated enumerated error codes. Omit AccountID, nonce,
OperationID, ChunkID, StorageId, peer/IP/hostnames, paths, run IDs and free-form
strings by default. Exact timestamps and network/block hashes need a stated
support purpose before inclusion. Bounded aggregation may be preferable to
record export; even projected counters may reveal activity patterns.
Never export vaults, recovery/operation journals, PoA signing history, ciphertext
or Application DB contents as part of routine diagnostics. Required incident
evidence follows a separate approved access and preservation procedure.

The UI/CLI must identify the included range, exclusions, source logging mode,
malformed/skipped counts and destination, and allow local review before sharing.
No current command satisfies this contract merely because it prints JSON.

## Records excluded from diagnostic expiry

PoA anti-equivocation signing history, network-bound canonical state, pending
Identity operation journals, vaults and recovery bridges are not diagnostic
segments. Their retention and recovery rules are separate. Diagnostic cleanup
must never traverse or modify those paths. Security-incident preservation must
have an explicit purpose, access scope and review/end condition, not an
unbounded exception for every log.

## Implementation and deployment gates

- Verify minimal filtering and forbidden field rejection with the existing
  EventWriter regression cases; separately inventory CLI/service output.
- Implement rollover with boundary/restart tests, Windows locked-file failures,
  disk/reopen errors, symlink/reparse rejection and unrelated-file preservation.
- Test export with adversarial values under allowed keys, malformed JSON,
  large input, secret markers, exact field projection and no network side effects.
- Read the deployed service unit, journald/rotation configuration and backup
  policy before claiming operational retention; record scope/date and source.
- Assign an accountable owner to purpose, durations, budgets and incident
  exceptions before enabling a release retention policy.

These gates do not introduce new logging CLI options, an exporter or a change
to PoA signing safety. Concrete implementation follows the adopted contract and
scoped acceptance evidence.

Local verification of existing behavior: the two Windows headless EventWriter
tests (`event_log_privacy_modes_filter_sensitive_identifiers` and
`public_event_writer_rejects_secret_fields`) passed, 13 assertions. They cover
mode filtering and field-name rejection, not live service retention, arbitrary
string-content redaction, rollover or support-export behavior.
