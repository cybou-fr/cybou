# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
cryptographic authorization rules. Commercial ownership does not confer access
to user private keys, plaintext content, or arbitrary account debits.

## Threat and resilience model

Network authority and operational roles remain strictly separated:

```text
Network Private Key offline:
    -> create / re-genesis with monotonic genesis_generation only

PoA key P:
    -> block finality only

Bootstrap peer:
    -> discovery + ordinary P2P only

Validator Identity (> 1M finalized Authority):
    -> advisory provisional Validation only
```

### Operational failures and compromise impact

- **Authority offline**: If the Central Authority desktop is offline, block finalization pauses. Full nodes continue serving existing state and chunks, but canonical state cannot advance.
- **Bootstrap resilience**: Bootstrap is an ordinary CYBOU full peer with a known locator. A bootstrap outage does not halt the network; connected mesh peers continue exchanging blocks, operations, validation attestations, and storage chunks directly.
- **Bootstrap compromise**: A hostile bootstrap can deny discovery or partition new connections, but cannot forge a genesis signed by the offline Network Private Key or forge PoA block certificates.
- **PoA compromise**: A stolen `P` threatens canonical block finality, but cannot sign a new genesis specification or alter the compiled Network Public Key (`NetworkID`).
- **Network Private Key compromise**: A stolen Network Private Key is a catastrophic root authority compromise, allowing the attacker to issue a valid newer genesis specification (`genesis_generation > installed_generation`). The private key must remain strictly offline at all times.
- **Validator compromise**: A compromised Identity with Authority > 1,000,000 can issue false advisory Validation attestations. This affects only provisional state on nodes with validation enabled; once PoA publishes a conflicting block, the provisional state is unconditionally rolled back.
- **Verification**: Every full node independently validates signatures, operation rules, and state roots. The operator cannot forge state transitions without detection.

Anti-equivocation protection is strictly enforced by local journals. If equivocation
is detected, all observing full nodes halt permanently.
