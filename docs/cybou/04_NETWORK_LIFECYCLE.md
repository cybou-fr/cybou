# 04 — Network lifecycle

Status: **Active architecture target**.
This document defines official network profiles, bootstrap rendezvous, network
creation, joining, monotonic replacement, full wipe semantics, Authority key
rotation, and direct peer discovery.

---

## 1. Official network profiles

A standard CYBOU installation initially knows explicit official network
profiles compiled into the software:

```text
DEVNET:
    bootstrap IP:port (51.255.46.58:29461)
    SPKI SHA-256 pin
    initial official PoA public key (K0)

TESTNET:
    bootstrap IP:port
    SPKI SHA-256 pin
    initial official PoA public key (K0)

MAINNET:
    bootstrap IP:port
    SPKI SHA-256 pin
    initial official PoA public key (K0)
```

Transport authentication and canonical truth are strictly separated:
- **IP:port** tells the client where to reach the rendezvous endpoint.
- **TLS SPKI pin** proves the client is talking to the intended bootstrap server (protects against MITM / network tampering).
- **Authority signature / trust chain** proves the advertised network state is official and valid.

Bootstrap is rendezvous and state distribution infrastructure. It is not a
consensus participant, has no voting power, and does not hold an Identity role.

---

## 2. Bootstrap lifecycle: EMPTY and BOUND

A bootstrap service holds one of two durable states:

1. `EMPTY`: No official network has been registered.
2. `BOUND`: An official network binding is active at generation $N$.

### Connecting to bootstrap

```text
CYBOU starts
   ↓
Connect known bootstrap (IP:port + SPKI pin)
   ↓
STATUS
   ├── EMPTY
   │     → Central Authority may create official network
   │
   └── BOUND
         → Receive current OfficialNetworkBinding
         → Verify binding through official Authority trust chain (starting at K0)
         → Compare generation N against local generation
```

---

## 3. Creating an official network

Network creation is permitted only when bootstrap is authenticated in the
`EMPTY` state.

1. **Prerequisites**:
   - Pinned TLS connection to bootstrap returning `EMPTY`.
   - Single-use bootstrap activation code (provisioned out-of-band by the operator).
   - Possession of the initial Central Authority PoA key $K_0$.
2. **Execution**:
   - The Central Authority desktop initializes genesis with initial parameters and finalizer public key $K_0$.
   - The desktop constructs the `OfficialNetworkBinding` (generation 1).
   - The binding is signed by $K_0$ and submitted to bootstrap along with the activation code.
   - Bootstrap atomically verifies the activation code, commits the binding to durable storage, and transitions to `BOUND` (generation 1).
   - The activation code is consumed permanently.

Bootstrap nodes never receive private keys. The Central Authority PoA key
remains exclusively on the operator desktop.

---

## 4. Joining an official network

When a client connects to a `BOUND` bootstrap:

1. The client receives the `OfficialNetworkBinding` and the exact network definition (genesis, state root, parameters).
2. The client verifies:
   - Network kind matches the local profile.
   - The binding is authentically signed by the current Authority key validly derived from compiled $K_0$.
   - The network definition hash matches the binding.
3. If valid, the client initializes local chain state from genesis and connects to initial peers.

---

## 5. Network replacement and full wipe semantics

If bootstrap advertises a newer generation $N > \text{local generation}$ under
a validly signed binding:

```text
bootstrap advertises generation N
local generation < N
+
binding verified by Authority trust chain
    ↓
STOP RUNTIME
    ↓
WIPE EVERYTHING
    ↓
install generation N
    ↓
start clean from new genesis
```

### Full wipe definition

Cross-network migration does not exist. A network replacement wipes all
network-bound local state completely:

- chain and block store
- state DB and state roots
- network definition and genesis
- Identity, vault, AccountID, recovery and authorization keys
- balances, System Balances, onboarding pool state
- registered `.cybou` names
- Mail, Files, application DB
- peer DB and address cache
- pending operations and volatile relay queues
- storage metadata, replica tracking, provider records
- Authority derived indexes

A clean network starts strictly from the new genesis. No state carries over.

---

## 6. Authority key rotation

Genesis starts the Authority trust chain at initial public key $K_0$. Operational
PoA signing keys may rotate monotonically without wiping the network:

```text
Genesis:
    Authority K0

Heights 1 .. h_1:
    Blocks signed by K0

Rotation record:
    K0 authorizes K1 (epoch 1)

Heights (h_1 + 1) .. h_2:
    Blocks signed by K1
```

### Rotation format

```text
AuthorityRotation {
    network_kind
    epoch
    previous_key_id
    new_public_key
    signature_by_previous
    proof_by_new
}
```

Clients and full nodes accept blocks from $K_{\text{epoch}}$ once the signed
rotation record is finalized. The chain of trust always roots in the compiled $K_0$.

---

## 7. Direct peer discovery and mesh operation

Bootstrap is an initial discovery rendezvous, not a permanent intermediary.

1. **Rendezvous**: Clients discover active full nodes from bootstrap's volatile peer cache.
2. **Mesh connection**: Peers connect directly to one another using CYP2 v3.
3. **Autonomy**: If bootstrap becomes unreachable, existing mesh peers continue syncing finalized blocks, relaying operations, and transferring storage chunks without interruption.
4. **Admission policy**: All public inbound and outbound P2P connections strictly follow the sovereign admission rules defined in [`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`](37_FRANCE_SOVEREIGN_NETWORK_POLICY.md).
