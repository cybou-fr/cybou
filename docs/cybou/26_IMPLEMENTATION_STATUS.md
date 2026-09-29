# Implementation status

## Canonical architecture

`main` targets one protocol: a genesis-bound hybrid-PQ PoA finalizer, full-node
validation, Identity-authorized RootPublication, and one encrypted,
content-addressed chunk tree for Mail and Files. Mail and Files are client-side
views over encrypted publications; neither has a separate consensus object or
indexed-object wire protocol. The chain does not implement BFT, ValidatorSet,
MailTx, dual operation decoders, automatic import, or runtime compatibility.

DEV has completed the coordinated reset and runs this protocol format. Git
history is the record of superseded implementations; they are not part of the
runtime. Routine deployment preserves active DEV state and validator keys.

## Present in source

- Hybrid identity keys and authorization, Identity KEM publication, names,
  recovery vault formats, and identity operation coordination.
- Deterministic block and state execution, finalized block storage, and a
  genesis-bound PoA certificate format and signer with durable anti-equivocation
  intent handling.
- Generic Identity-authorized RootPublication with canonical CBOR encoding,
  bounded recipient capsules, network-bound deterministic fees, and finalized
  history lookup.
- Encrypted content-addressed chunks, a streaming ROOT/INDEX/DATA tree,
  inclusion proofs, durable local provider admission, and local reconstruction
  primitives.
- CYP2 peer sessions for verified block sync and content-addressed chunk PUT/GET
  with inclusion proofs. Legacy `StorageObject` runtime APIs, implementation,
  and wire messages have been physically removed; prior wire IDs are rejected.

These components do not yet establish a usable Mail or Drive product. The
client still needs publication construction and scanning, recursive retrieval,
recipient-facing Mail/Files views, and clean-machine recovery from Identity
capabilities and published content.

## Remaining integration work

- Complete cross-platform builds and cross-implementation vectors for the
  canonical wire formats and hybrid cryptographic paths.
- Exercise PoA journal recovery, conflict evidence and full-node state sync
  across clean machines and restart/failure scenarios. Runtime block acceptance
  and production already enforce the durable equivocation safety halt.
- Migrate desktop storage calls to chunk-ID admission and publication inclusion
  proofs, then implement provider placement, durability thresholds, retry,
  retention, repair, and provider-loss handling.
- Integrate publication creation, Mail and Files scanning, recipient capsule
  opening, recursive chunk retrieval, local indexes, and clean-machine restore.
- Finish Gmail-familiar Mail and Google Drive-familiar Files UI/UX acceptance.
- Validate integrated Mail, Files and recovery paths on DEV without another
  reset. Any future incompatible protocol change needs its own explicit cutover.

Do not add a compatibility layer or preserve old chain state to make the reset
appear incremental. Update this status from source and operational evidence as
each gate closes.
