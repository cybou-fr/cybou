# 23 — Manual acceptance v0.0.1

## Network quarantine
```text
[ ] cybou.exe starts
[ ] separate CYBOU datadir
[ ] no Bitcoin peers/seeds/blocks/UI/URI
```

## Typed protocol operations
```text
[ ] MailTx is an explicit CYBOU operation type
[ ] MailTx is not OP_RETURN/application blob abuse
[ ] canonical serialization deterministic
[ ] unknown protocol operation version/type rejected
[ ] oversized MailTx rejected
[ ] priority fee path disabled
```

## Account Creation and Onboarding
```text
[ ] permissionless AccountCreateOpV1 accepted
[ ] invalid or insufficient AccountCreationWork rejected
[ ] work_epoch is derived from block height and stale/future work is rejected
[ ] max_account_creates_per_block is enforced
[ ] duplicate AccountID creation rejected
[ ] valid creation credits onboarding bonus OnboardingPool -> SystemBalance
[ ] identity creation alone does not mint tokens
```

## Canonical state commit
```text
[ ] genesis initialization cannot overwrite existing canonical state
[ ] genesis persists the NetworkID derived from the immutable network definition
[ ] reopening state under a different NetworkID is rejected before mutation
[ ] unsupported or structurally invalid network definitions cannot initialize or advance state
[ ] caller cannot arbitrarily replace canonical state
[ ] invalid block operation leaves canonical state/root/tip unchanged
[ ] finalized child commits state, state root, tip and height atomically
[ ] non-contiguous finalized height is rejected without state mutation
[ ] duplicate finalized block is rejected
[ ] block whose parent is not the finalized tip is rejected
[ ] no CYBOU production rollback or per-block undo path exists
```

## Mail crypto
```text
[ ] content commitment uses random salt
[ ] plaintext subject/body absent from chain
[ ] sender signature verifies
[ ] recipient key package authenticated
[ ] modified ciphertext rejected
[ ] downgrade rejected
```

## Mail finality/offline receive
```text
[ ] Bob offline during send/finality
[ ] Alice MailTx finalizes
[ ] no permanent MailMarker is added to current state
[ ] Bob later discovers relevant block/range
[ ] Bob obtains historical ciphertext
[ ] Bob verifies inclusion/finality/signature
[ ] Bob decrypts locally
```

## Pre-Store retention
```text
[ ] desktop node can prune according to policy
[ ] active validator retains required pre-Store MailTx history
[ ] historical recipient can retrieve ciphertext from validator/archive path
[ ] product does not claim state hash can reconstruct pruned ciphertext
```

## PoT
```text
[ ] epoch derived deterministically from finalized height
[ ] 25 MailTx/epoch enforced for new account
[ ] local wall clock/timezone changes do not affect consensus result
[ ] PoT arithmetic is integer/deterministic
```

## BFT
```text
[ ] 1-validator dev mode works
[ ] 2–3 validator integration works
[ ] 4-validator f=1 scenarios simulated
[ ] equal vote weight
[ ] operator-authorized activation/removal is chain-visible
[ ] conflicting finalization safety tests pass
[ ] finalized CYBOU blocks cannot be reorged through the state-store API
```

## Evidence
```text
[ ] MailEvidenceBundle export
[ ] tx inclusion proof verifies
[ ] finality certificate verifies
[ ] historical sender-key authorization verifies
[ ] plaintext + disclosed salt recomputes ContentCommitment
```

## Beta consensus hardening acceptance

- Reopening state with a different Operator Authority keyset or MailTx quota
  must fail with a network mismatch.
- A validator whose ID differs from its consensus public key must sign and
  verify BFT messages using the configured consensus key.
- A proposal with invalid operations or a mismatched state root must receive
  no positive prevote, lock, precommit, or finalization.
- A validator-set transition must refresh the running node's set and index
  before voting at the next height.
- A new account with the DEV 6,000 SystemBalance bonus retains the baseline
  25 MailTx limit per epoch.
