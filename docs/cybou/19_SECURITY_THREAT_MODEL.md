# 19 — Security and threat model

## Trust boundary

The network definition binds one hybrid-PQ PoA finalizer key. That operator
controls block ordering and can censor or stop progress. PoA is centralized
finality, not BFT. Every full node independently checks the finality
certificate, parent, height, operation authorization, deterministic state
transition, and state root. Operator equivocation or journal rollback is a
network safety incident and must halt verification.

Finalized RootPublication authorizes chunk admission; it does not prove that
any provider retained or can serve a chunk. Clients must distinguish finalized,
available, retrievable, and protected states.

## Security goals

Protect against:

- malicious or malformed peers and invalid blocks;
- unauthorized Identity operations, replay, and key substitution;
- storage providers learning plaintext or private application schemas;
- corrupted, missing, or falsely claimed stored content;
- network spam and resource-exhaustion inputs;
- protocol downgrade and noncanonical serialization;
- accidental connection to Bitcoin networks;
- malicious snapshots and supply-chain or release compromise.

## Explicit limitations

CYBOU does not claim:

- progress while the PoA operator is offline;
- censorship resistance against the PoA operator;
- BFT or independent-operator fault tolerance;
- guaranteed physical or administrative independence of storage peers;
- production durability before measured provider operation;
- recovery after loss of every authorized Identity secret.

## Invariants

1. Network quarantine precedes manual desktop launch.
2. Application content is encrypted before leaving the client.
3. Providers receive opaque, content-addressed ciphertext and admission proofs.
4. Full nodes verify canonical bounded encodings and execute state transitions locally.
5. Every production signature follows the required hybrid post-quantum key policy.
6. Network input lengths and tree traversal are bounded before allocation or output.
7. Desktop operation does not require a local HTTP/RPC administrative surface.
8. Release authenticity is independently verifiable.
9. A claimed disk capacity alone never grants protocol entitlement.

## Identity key loss

Identity recovery uses the user's 24-word phrase and portable encrypted vault.
Loss of both makes protected content unrecoverable unless a separately reviewed
recovery mechanism exists. Recovery and clean-machine restore requirements are
defined in `76_IDENTITY_VAULT_RECOVERY.md` and
`IDENTITY_DISCOVERY_AND_RECOVERY.md`.
