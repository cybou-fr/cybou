# Identity publication discovery and recovery

Status: frozen architecture target; local index format and sync bounds remain
open. This replaces service-specific network discovery filters after the
coordinated protocol cutover.

Every client scans finalized RootPublications from its saved height. For each
recipient capsule it tries the account's current or explicitly recoverable
Identity KEM epochs. A failed capsule is recorded locally as `NOT_FOR_ME`; a
successful unwrap yields the root key and accessible RootChunkID. No recipient
ID or Mail/Files type is required in chain state.

The local encrypted database stores the last scanned height, operation ID,
capsule outcome, accessible root/key envelope, and application indexes. It is
a rebuildable cache, not protocol identity or the only recovery source. Re-scan
policy after Identity rotation, old-epoch availability, bounds on per-block
capsules, and denial-of-service controls are cutover gates.

Clean-machine recovery acceptance starts from only the mnemonic and public
network definition: restore Identity roles, sync and independently validate
finalized history, scan publications, fetch authorized chunks, verify IDs,
decrypt bounded CBOR, and rebuild Mail/Files catalogs. No prior client DB,
Storage sidecar, or special recovery server may be required. Recovery
availability and key-epoch policy must be specified before Beta.

\n