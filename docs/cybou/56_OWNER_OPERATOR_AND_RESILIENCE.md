# 56 — Owner, operator, and network trust

CYBOU is commercially owned and operated by CYBOU. User Identity, balances,
names, and encrypted content remain protocol-owned under their respective
cryptographic authorization rules. Commercial ownership does not confer access
to user private keys, plaintext content, or arbitrary account debits.

## Threat and resilience model

Network authority and operational roles remain strictly separated:

```text
Network Private Key offline:
    -> create and sign the immutable genesis ONCE only

Ordinary cybou.cybou Identity's PoA key role P:
    -> block finality only

Bootstrap peer:
    -> discovery + ordinary P2P only
```

### Operational failures and compromise impact

- **Authority offline**: If the Central Authority desktop is offline, block finalization pauses. Full nodes continue serving existing state and chunks, but canonical state cannot advance.
- **Bootstrap resilience**: Bootstrap is an ordinary CYBOU full peer with a known locator. A bootstrap outage does not halt the network; connected mesh peers continue exchanging blocks, operations and storage chunks directly.
- **Bootstrap compromise**: A hostile bootstrap can deny discovery or partition new connections, but cannot forge a genesis signed by the offline Network Private Key or forge PoA block certificates.
- **PoA compromise**: A stolen `P` threatens canonical block finality, triggering equivocation safety halt across observing nodes. The current network cannot safely continue and requires launching a new NetworkID / genesis cutover. A stolen `P` cannot sign a genesis specification or alter the compiled Network Public Key (`NetworkID`).
- **Network Private Key compromise**: The Network Private Key signs the immutable genesis once at network creation. Existing official binaries compile that exact signed genesis and initial state and accept no external official replacement, so a stolen key cannot update them in place. The private key remains strictly offline under gitignored `/private/` during provisioning.
- **Verification**: Every full node independently validates signatures, operation rules, and state roots. The operator cannot forge state transitions without detection.

Anti-equivocation protection is strictly enforced by local journals. If equivocation
is detected, all observing full nodes halt permanently.
