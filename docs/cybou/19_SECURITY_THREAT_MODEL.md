# 19 — Security and threat model

## Security goals

Protect against:

- malicious peers;
- invalid blocks/transactions;
- storage-node plaintext access;
- corrupted storage;
- false capacity claims;
- replayed storage proofs;
- network spam;
- downgrade attacks;
- compromised individual storage nodes;
- accidental connection to Bitcoin networks;
- malicious snapshots;
- supply-chain/release compromise.

## Important non-goals / unresolved guarantees

CYBOU does not yet claim:

- recovery after loss of all user/device keys without any additional trust mechanism;
- resistance to a majority/2/3+ consensus compromise beyond the selected BFT model;
- perfect traffic-analysis resistance;
- guaranteed physical-geography independence of anonymous peers;
- proven production durability parameters before simulation/field data.

## Critical invariants

1. Bitcoin network quarantine precedes first manual desktop launch.
2. Plaintext application data is encrypted before storage-network transmission.
3. Storage peers do not receive plaintext semantic metadata.
4. State snapshots are verified against trusted/finalized commitments.
5. Consensus-critical serialization is canonical and bounded.
6. Crypto negotiation is downgrade-protected.
7. Network input parsing is length-bounded before allocation.
8. No local HTTP/RPC administrative surface is required for desktop operation.
9. Release authenticity is independently verifiable.
10. A configured disk-size claim alone earns no storage entitlement.

## Key-loss reality

Identity V2 targets user-held 24-word Recovery Root, portable encrypted vault,
and replaceable devices. The current DEV runtime does not implement these.
Compromise of a device must be contained by finalized revocation; compromise
of a recovery phrase requires root rotation. Offline password guessing against
stolen vaults, tampered headers, key substitution, mnemonic transcription,
and loss of the phrase remain explicit threats. See `76_IDENTITY_VAULT_RECOVERY.md`.

Without:

- a provider recovery authority/KMS;
- a user-held recovery secret;
- or another explicit social/multi-party recovery mechanism,

loss of all authorized private keys can make encrypted data unrecoverable.

This remains a production blocker to solve honestly.
