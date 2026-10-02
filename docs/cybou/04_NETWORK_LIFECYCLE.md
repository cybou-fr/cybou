# 04 — Network lifecycle

Status: **Active architecture target**. This document defines official network
trust, creation, joining, Authority rotation, network replacement and discovery.

## Official network profile and binding

A standard installation knows explicit DEVNET, TESTNET and MAINNET profiles.
Each `OfficialNetworkProfile` contains bootstrap IP:port locator(s), their TLS
SPKI SHA-256 pins, and one immutable Network Root public key `R`. The DEV
locator is `51.255.46.58:29461`; its pin authenticates transport only.

`R` signs an `OfficialNetworkBinding` containing the profile/network kind,
monotonic `generation`, `authority_epoch`, exact network definition (including
genesis and its hash), and current PoA public key `P`. The binding also commits
to the Authority assignment `{epoch, activation_height, P}`. The root signature
is the proof of official network and Authority assignment. A pinned bootstrap
can distribute a binding but cannot make one official. `R` never signs blocks;
`P` cannot replace the network or authorize another `P`.

Bootstrap is an `EMPTY` or `BOUND` rendezvous/state distribution service. It is
not a CYP2 peer role, consensus participant or Identity entity. `EMPTY` has no
binding. `BOUND` serves the current root-signed binding and initial peer hints.

## Creation and joining

```text
EMPTY bootstrap → operator creates genesis and P0
                → R signs generation 1 binding and epoch 0 assignment
                → activation code + binding submitted to bootstrap
                → bootstrap verifies and durably enters BOUND
```

The one-use bootstrap activation code controls initial store activation, not
network authenticity. Bootstrap receives no private keys. The operational PoA
private key resides on the Central Authority desktop; private `R` remains
outside routine finalizer execution.

```text
profile → pinned bootstrap → root-signed OfficialNetworkBinding
        → verified genesis → initial peers → direct CYP2 mesh
```

The core checks the profile, root signature, binding fields, monotonic counters,
exact network definition hash and genesis before opening network-bound local
state. A peer address, claimed height, TLS pin or successful sync is never a
substitute for this verification. All full nodes verify subsequent finalized
blocks and state transitions independently.

## Authority rotation within one generation

A rotation increments `authority_epoch` from E to E+1. `R` signs the new
assignment `{epoch, activation_height, P}` and updated binding. The old and
new epochs have unambiguous height ranges. Nodes retain the minimum root-signed
assignment history needed to verify historical certificates. They reject
rollback, gaps, overlapping assignments and a key change without a valid root
signature. A previous operational PoA key never signs its successor into
authority. Rotation preserves the network, genesis and all Identity/application
state. Signing journal semantics are specified in `POA_FINALITY.md`.

## New generation and full replacement

A binding for generation N+1 is a new official network, even if issued for the
same profile. The core verifies the entire new binding and definition before
switching. It stops network services, prepares and atomically activates the
new network root, then permanently removes the old network-bound domain. A
failed preparation leaves the old generation intact; a crash during activation
must recover to a single generation, never a mixture.

The old domain includes chain/state, definition, genesis, Identity, vault,
AccountID, Recovery/Authorization/KEM keys, balances, names, Mail, Files,
Application DB, peer DB, pending operations, storage metadata and Authority
indexes. No cross-network Identity or content migration exists. Application
preferences outside the network domain, such as theme, language and validated
Geo cache, may remain. A client must never open an old vault against the new
network. Equal or lower generation cannot trigger replacement.

## Direct mesh

Bootstrap provides initial peer hints. Ordinary full nodes then exchange
finalized blocks, bounded volatile operation relays and authorized encrypted
chunks directly. A bootstrap outage does not stop an already formed mesh.
Production and DEV public inbound/outbound admission is France-only and fails
closed when local Geo data is unavailable or corrupt; see
`37_FRANCE_SOVEREIGN_NETWORK_POLICY.md`.
