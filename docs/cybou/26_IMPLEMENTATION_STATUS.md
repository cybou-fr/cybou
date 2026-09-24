# Implementation status

CYBOU is experimental. The canonical product target uses hybrid post-quantum authorization, explicit BFT finality, and one verified state shared by Identity, Email, Wallet, Storage, and Backup. The current DEV node and Qt desktop are still being connected to that target. A development reset will follow the integration of identity, names, operations, blocks, and persistence.

## Implemented core components

- Random stable AccountID, independent of mnemonic and keys.
- 24-word recovery phrase encoding and recovery-derived Ed25519 + ML-DSA-65 root keys.
- Independent Ed25519 + ML-DSA-44 device keys and versioned RecoveryKeyID and DeviceKeyID commitments.
- Portable encrypted CYBV2 vault with Argon2id and AES-256-GCM, durable create-only save, and reopen verification.
- Canonical account creation with anti-Sybil work and hybrid root/device proofs of possession.
- Bounded device registry with add, revoke, root rotation, independent device nonces, and activation numbers that prevent replay after a key is re-added.
- Canonical identity-registry and monetary-state snapshots with a domain-separated state root.
- Account creation that moves the onboarding bonus from OnboardingPool to SystemBalance.
- Device-authorized payments with deterministic fees, overflow checks, and atomic nonce/balance updates on candidate state.
- Versioned AccountCreate and Payment wire encodings, operation IDs, and a candidate block executor that routes four fee units as three Security plus one Onboarding.
- Validator-set validation, hybrid validator signatures, BFT finality certificates, and a core consensus engine with explicit finality.
- Canonical operations, name registry, block execution, state store, and standalone authority/observer sync are connected to the native node runtime.
- The DEV network definition commits to the active name rules. The CLI derives genesis validator keys from the same secret used by the producer; the desktop loads the verified network file.

## Integration still required

- Integrate random AccountID, 24-word phrase confirmation, portable vault, and clean-machine restore into the desktop identity service. The current seed-based keystore still derives AccountID from Ed25519.
- Complete Mail confidentiality with independent X25519 and ML-KEM keys, usable recipient discovery, encrypted local mailbox storage, and historical sender-key authorization evidence.
- Complete desktop `.cybou` claim and finalized ownership flows on top of the implemented registry.
- Move the Qt creation, restore, wallet, and Mail flows onto the canonical runtime and portable vault.
- Run independent validators with durable crash recovery and verify finality under production topology.
- Finish operator, release, and treasury signing integration under the PQ key policy.
- Implement distributed Object Storage and Backup before large attachments and mass-scale Mail.
- Remove obsolete runtime paths, names, files, and documentation before the DEV reset. No compatibility decoder or automatic state/vault import is planned.

## Current network boundary

The available development producer runs one validator in Authority Mode (`f=0`). Four equal-weight validators are required to claim tolerance of one Byzantine fault. The bounded DEV transport and compiled bootstrap endpoint are integration tools, not a production peer-to-peer network.

The development network can be reset. Do not treat DEV identities, balances, validator keys, or network state as production assets.
