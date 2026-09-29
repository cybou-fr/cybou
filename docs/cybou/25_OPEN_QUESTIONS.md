# Open engineering and product gates

The active PoA and streaming encrypted chunk-tree protocol is deployed on DEV.
This list tracks remaining validation and product readiness, not a pending
protocol reset. Product details belong in the Mail and Files UX documents;
wire and state authority belongs in the active protocol documents listed by
`AGENTS.md`.

## Finality and state

- Expand canonical hybrid signature/finality vectors across supported
  implementations and verify independently recomputed finalized state on a
  clean full node.
- Document and exercise operator recovery after a PoA safety halt; conflicts
  already fail closed and never trigger automatic fork selection.

## Encrypted content and admission

- Cross-implementation canonical-CBOR and streaming ROOT/INDEX/DATA tree vectors.
- Reviewed vectors for X-Wing draft-05 recipient capsule wrapping and recovery.
- Cross-implementation RootPublication execution, replay protection, and exact
  fee/state accounting.
- Cross-implementation provider proof validation against finalized
  RootPublication data.
- Durability threshold, independent per-chunk placement, retry, retention,
  audits, repair, accounting, and provider-loss handling.
- Publication scanning and recursive retrieval from a clean client without a
  previous local database.

## Product and operations

- Gmail-familiar Mail and Google Drive-familiar Files UI acceptance on desktop.
- Offline-recipient retrieval and recovery after interrupted transfers.
- Beta operational cost measurements for Mail, Files/Storage, and onboarding.
- Legal review of product claims and evidence exports before public service.

These are integration and Beta gates on the active DEV protocol. They do not
require another reset unless a future incompatible protocol format is adopted.
