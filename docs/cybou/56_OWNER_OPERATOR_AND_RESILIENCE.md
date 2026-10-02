# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
cryptographic authorization rules. Commercial ownership does not confer access
to user private keys, plaintext content, or arbitrary account debits.

## Operational model

The network uses single-operator hybrid-PQ PoA finality operated by the
Central Authority Identity from its desktop.
- **Authority offline**: If the Central Authority desktop is offline, block finalization pauses. Full nodes continue serving existing state and chunks, but canonical state cannot advance.
- **Bootstrap resilience**: Bootstrap is discovery rendezvous infrastructure. A bootstrap outage does not halt the network; connected mesh peers continue exchanging blocks, operations, and storage chunks directly.
- **Verification**: Every full node independently validates signatures, operation rules, and state roots. The operator cannot forge state transitions without detection.

Anti-equivocation protection is strictly enforced by local journals. If equivocation
is detected, all observing full nodes halt permanently.
