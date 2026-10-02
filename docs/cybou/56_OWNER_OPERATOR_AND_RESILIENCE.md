# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
cryptographic authorization rules. Commercial ownership does not confer access
to user private keys, plaintext content, or arbitrary account debits.

## Operational model

The network uses single-operator hybrid-PQ PoA finality operated by the
Central Authority desktop under current root-authorized key `P`. Network Root
`R` separately authorizes network bindings and Authority assignments; its
private key is outside routine finalization.
- **Authority offline**: If the Central Authority desktop is offline, block finalization pauses. Full nodes continue serving existing state and chunks, but canonical state cannot advance.
- **Bootstrap resilience**: Bootstrap is discovery rendezvous infrastructure. A bootstrap outage does not halt the network; connected mesh peers continue exchanging blocks, operations, and storage chunks directly.
- **Bootstrap compromise**: A hostile bootstrap can deny discovery or lie about peer availability, but cannot create a binding accepted under `R`.
- **PoA compromise**: A stolen `P` threatens the current epoch's finality but cannot replace the network or appoint a successor.
- **Root compromise**: A stolen `R` compromises official network authority and requires a separately planned recovery process.
- **Verification**: Every full node independently validates signatures, operation rules, and state roots. The operator cannot forge state transitions without detection.

Anti-equivocation protection is strictly enforced by local journals. If equivocation
is detected, all observing full nodes halt permanently.
