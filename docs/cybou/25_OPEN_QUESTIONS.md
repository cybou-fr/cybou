# 25 — Open questions / blockers v0.0.1

## Identity V2 parameters to freeze before coding/activation

- Exact standardized 24-word list, normalization, checksum, KDF paths and
  deterministic Ed25519/ML-DSA test vectors.
- CYBV2 canonical encoding, Argon2id resource bounds, file permissions and
  cross-platform atomic replace behavior.
- Hybrid signature wire bounds and canonical signed bytes for each operation.
- Bounded device count and recovery-rotation conflict rules.
- Name reserved-list version, commit lifetime/depth, work target, salt length,
  pending-commit cap, fees, and canonical ordering.

## Mail / privacy

### O-001 Recipient discovery tag
Design the recipient tag so clients can find relevant MailTx without publishing avoidable social-graph metadata.

### O-002 Compact mail discovery filter
Choose a deterministic block/range filter format.

### O-003 MailTx maximum size / fee tiers
Benchmark actual PQ/T overhead before freezing byte thresholds.

### O-004 Sender privacy
Decide whether sender AccountID is public/pseudonymous in v1 or whether stronger unlinkability is required.

### O-005 Multi-device encryption
Freeze recipient key-capsule semantics.

### O-006 Forward secrecy / rotating prekeys
Decide the stronger compromise model after the base HPKE/PQ profile works.

## Pre-Store retention

### O-007 Historical validator retention
Define exact canonical data range and serving protocol required from active validators before Store.

### O-008 Historical migration
Decide whether pre-Store MailTx bodies are later migrated into Store or remain validator/archive history.

## BFT / operator authority

### O-009 Exact BFT protocol — resolved
Frozen as Tendermint/IBFT style N-validator state machine (N >= 1) with deterministic round leader `(height + round) % N`, prevote/precommit locking, quorum `floor(2*N/3)+1` forming `BftFinalityCertificateV1`, and atomic `CommitFinalizedBlock` validation in `CybouStateStore`. Authority Mode (N=1) has 1/1 finality and f=0; f=1 requires at least four validators. Verified in `cybou::BftSimulator`.

### O-010 Validator admission transaction format — resolved
Frozen as typed `ValidatorAdmissionOpV1` and `ValidatorRemovalOpV1` protocol operations with domain-separated hybrid `Ed25519 + ML-DSA-65` Operator Authority signatures (`CYBOU/SIG/VALIDATOR-ADMISSION/V1` and `CYBOU/SIG/VALIDATOR-REMOVAL/V1`), binding `NetworkID`. Active validator set is tracked directly in canonical consensus state (`CybouState`) with equal weight = 1, unique validator IDs and consensus keys, and non-empty active set invariants enforced by `CybouStateStore`.

### O-011 Emergency operator succession
Define operator-unavailable recovery without normal community governance.

### O-012 Reward epoch
Freeze validator reward epoch length.

## PoT / invite

### O-013 PoT epoch length
Choose `EPOCH_BLOCKS` after real block cadence is known.

### O-014 Exact PoT score
Freeze capped System Balance, age, clean-history, activity and penalty contributions.

### O-015 Account Creation anti-Sybil difficulty tuning
Calibrate the initial PoW target bits and dynamic difficulty adjustment for `AccountCreationWorkV1`.

### O-021 Operator Authority signature suite
Benchmark and review a domain-specific hybrid signature profile for rare
Operator Authority operations. `Ed25519 + ML-DSA-65`, requiring both component
signatures to verify, is a candidate rather than a frozen decision. Freeze the
suite identifier, exact signed bytes, component-key binding, downgrade rules,
and implementation backend together. Do not copy a changing Internet-Draft wire
format into consensus.

### O-022 AccountID and network identifiers — resolved
AccountID V1 is an opaque nonzero 32-byte identifier. AccountCreationWork network_id
is the domain-separated hash of the canonical immutable network definition.
Both use the internal byte order frozen by the V1 canonical serializer; see
DEC-156 and DEC-161.

### O-023 Validator archival mode enforcement
Define how a node declares and proves the active-validator role so the software
can reject pruning configurations while pre-Store historical MailTx retention
is mandatory.

### O-024 Account authorization and proof of possession — resolved
Frozen: Account authorization uses Ed25519; `AccountCreateOpV1` enforces cryptographic Proof of Possession (`CYBOU/ACCOUNT_POP/V1`) over `network_id || account_id || authorization_key`. Verified in `ValidateAccountCreateOp`.

## Evidence / legal

### O-016 Consensus timestamp semantics
Define validator/block time drift rules if a network timestamp is exposed as evidence.

### O-017 Legal evidence positioning
Obtain French/EU review before marketing the cryptographic record as notarization or registered-mail equivalent.

## Storage later

### O-018 Store mail-object format
Freeze encrypted body/attachment manifest.

### O-019 Storage provider verification
Required before storage rewards.

### O-020 Fee Router v2
Define provider share only when Store exists.
