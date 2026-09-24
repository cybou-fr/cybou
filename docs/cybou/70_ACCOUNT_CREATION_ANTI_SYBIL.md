# 70 — Account Creation Anti-Sybil Architecture

## Status

This document defines the permissionless, protocol-native account creation mechanism for CYBOU.
The centralized Operator-signed Voucher architecture has been permanently decommissioned.
Users create accounts natively through the consensus protocol without central authorization, permission, or invites.

## Principles

1. **Permissionless Access**: Any user or client node may create an AccountID. No operator signatures, vouchers, or approval gates are permitted.
2. **Anti-Sybil Proof-of-Work**: To prevent mass account registration and exhaust of the `OnboardingPool`, every account creation operation must include valid work satisfying the network's difficulty parameter.
3. **Atomic Onboarding Bonus**: Upon first creation of an account, an onboarding bonus is transferred atomically from `OnboardingPool` directly to `SystemBalance`. Identity creation alone never mints new tokens.
4. **Authority Separation**: Operator Authority keys are restricted to validator admissions and removals. They MUST NOT participate in ordinary user account creation or onboarding.

## Canonical Data Structures

### AccountCreationWorkV1

Proof-of-work bound strictly to the target account, network, and initial authorization commitment:

```text
AccountCreationWorkV1 {
    version: uint8 (1)
    network_id: uint256
    account_id: AccountId (32 bytes)
    initial_authorization_commitment: uint256 (32 bytes)
    work_epoch: uint64 (8 bytes LE)
    nonce: uint64 (8 bytes LE)
}
```

Total canonical serialized size: 113 bytes.

Canonical hashing rule:

```text
WorkHash = SHA-256("CYBOU/ACCOUNT-CREATE-WORK/V1" || canonical_serialized_work)
```

The computed hash must satisfy the network difficulty target (e.g. minimum leading zero bits).

### AccountCreateOpV1

Consensus operation submitted to validators:

```text
AccountCreateOpV1 {
    version: uint8 (1)
    account_id: AccountId
    initial_authorization: AccountAuthorizationV1
    creation_work: AccountCreationWorkV1
    proof_of_possession: Ed25519 signature (64 bytes)
}
```

The proof of possession signs the domain-separated digest
`SHA256("CYBOU/ACCOUNT_POP/V1" || NetworkID || AccountID || authorization_key)`.
It prevents registration of an account under a key the creator does not
control. `AccountCreateOpV1` is canonical DEV code; its wire profile remains
subject to versioned protocol review.

## Validation and State Transition

When processing `AccountCreateOpV1`:

1. **Op Integrity**: Validate version, non-null fields, work difficulty, and
   proof of possession of the initial authorization key.
2. **Network Binding**: `creation_work.network_id` must match the active NetworkID derived from the immutable `CybouNetworkDefinitionV1`.
3. **Account Binding**: `creation_work.account_id` must match `op.account_id`.
4. **Auth Binding**: `creation_work.initial_authorization_commitment` must match `ComputeAuthCommitment(op.initial_authorization)`.
5. **Epoch Binding**: The valid work epoch is derived deterministically from finalized block height and immutable network parameters; caller-supplied wall-clock time is irrelevant.
6. **Anti-Duplication**: `op.account_id` must not already exist in consensus state.
7. **Pool Solvency**: `OnboardingPool` must hold sufficient funds for the network bonus.
8. **Block Limit**: The block must not exceed `max_account_creates_per_block`.
9. **Candidate State Transition**:
   - `OnboardingPool` debited by onboarding bonus.
   - New `AccountState` created with `balance = 0`, `system_balance = onboarding_bonus`, `creation_height = height`, `creation_epoch = epoch`, and the active authorization key. The work commitment is not retained as a separate account-state field.
   - Any invalid operation rejects the candidate without changing canonical state.
10. **Finalized Commit**: After BFT finality, candidate state, state root, finalized tip and finalized height are persisted atomically. The height must advance by exactly one. Finalized CYBOU state has no production rollback or per-block undo path.

## Network Economics and Rate Limits

- **Dev Testnet Bonus**: 6,000 CYBOU.
- **Beta / Mainnet Bonus**: Parameterized and determined by network launch conditions.
- **Block Limit**: `max_account_creates_per_block` caps consensus throughput for new identities.
- **Epoch Rate Limits**: New accounts are constrained by Proof of Trust (PoT) outgoing MailTx limits per epoch.
