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
}
```

## Validation and State Transition

When processing `AccountCreateOpV1`:

1. **Op Integrity**: Validate version, non-null fields, and work difficulty.
2. **Network Binding**: `creation_work.network_id` must match the active network ID (genesis block hash).
3. **Account Binding**: `creation_work.account_id` must match `op.account_id`.
4. **Auth Binding**: `creation_work.initial_authorization_commitment` must match `ComputeAuthCommitment(op.initial_authorization)`.
5. **Anti-Duplication**: `op.account_id` must not already exist in consensus state.
6. **Pool Solvency**: `OnboardingPool` must hold sufficient funds for the network bonus.
7. **Atomic State Mutation**:
   - `OnboardingPool` debited by onboarding bonus.
   - New `AccountState` created with `balance = 0`, `system_balance = onboarding_bonus`, `creation_height = height`, `creation_epoch = epoch`, `initial_auth_commitment`.
   - Undo delta recorded for chain rollback.

## Network Economics and Rate Limits

- **Dev Testnet Bonus**: 6,000 CYBOU.
- **Beta / Mainnet Bonus**: Parameterized and determined by network launch conditions.
- **Block Limit**: `max_account_creates_per_block` caps consensus throughput for new identities.
- **Epoch Rate Limits**: New accounts are constrained by Proof of Trust (PoT) outgoing MailTx limits per epoch.
