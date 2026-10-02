# Implementation status

Status: **Head commit `f703423`** (`bootstrap: pin the DEV VPS locator`).

## Current DEV deployment

- **Service**: `cybou-bootstrap.service` runs `cybou-bootstrap serve` on `51.255.46.58:29461` (port 29461/TCP).
- **TLS & Pinning**: Pinned TLS SPKI SHA-256 is compiled into `src/cybou/bootstrap_nodes.h`.
- **State**: Durable LevelDB `EMPTY`/`BOUND` state under `/var/lib/cybou/bootstrap/state`. Currently `EMPTY`.
- **Legacy services**: Legacy `cybou-node.service` PoA finalizer and provider services are completely inactive.
- **Authority**: No Central Authority PoA finalizer is running on the VPS. PoA finalization targets operator desktop execution.

## Implemented

- **Core consensus**:
  - Deterministic state execution, balances, System Balances, onboarding pool.
  - Genesis-bound hybrid-PQ PoA finality (Ed25519 + ML-DSA-65).
  - Durable PoA anti-equivocation journal and conflict safety halt.
  - Operation validation and canonical state roots.
- **Identity & Vault**:
  - Stable random AccountID independent of mnemonic.
  - Separate key roles: Recovery (Ed25519 + ML-DSA-65), Authorization (Ed25519 + ML-DSA-44), KEM.
  - CVID5 vault serialization and password-protected encryption.
  - Identity rotation and recovery bridge.
- **Content & Storage**:
  - Generic RootPublication and encrypted recipient capsules.
  - Encrypted ROOT/INDEX/DATA tree; ChunkID is BLAKE3-256 of encrypted bytes.
  - Merkle proofs for post-finality chunk admission.
  - Local ChunkStore, provider PUT/GET over CYP2, multi-replica placement.
- **Transport & P2P**:
  - CYP2 v3 over TLS 1.3 (`X25519MLKEM768`).
  - Hop-by-hop operation relay with bounded volatile queues and loop suppression.
  - France-only sovereign peer admission policy with DB-IP Lite validation and fail-closed checks.
- **Bootstrap prototype**:
  - `cybou-bootstrap` utility with `EMPTY`/`BOUND` LevelDB store and one-use activation code.
  - SPKI pin verification for pre-genesis transport security.

## Incomplete / Next integration gates

1. **Official DEV bootstrap lifecycle**: Complete desktop locator client to talk to `51.255.46.58:29461` with pinned SPKI.
2. **Create DEV network from desktop**: Desktop GUI/CLI workflow to initialize genesis $K_0$, bind generation 1 with activation code, and set DEV bootstrap to `BOUND`.
3. **Bootstrap → first peer discovery**: Retrieve active peer addresses from bootstrap and transition to direct CYP2 mesh.
4. **Desktop Central Authority**: Wire PoA finalizer signing into unlocked operator desktop session.
5. **Full wipe enforcement**: On detecting newer valid official generation, trigger clean wipe of all local network state and join new genesis.
6. **Windows / Linux desktop acceptance**: End-to-end integration tests for clean install, network sync, Mail, and Files.
7. **Storage durability**: Advance from development (1 replica) to Beta target (2 independent remote replicas).

## Deferred / Non-Beta

- Advisory Validation attestations (no wire format or canonical state needed for launch).
- Erasure coding (full replication used for Beta).
