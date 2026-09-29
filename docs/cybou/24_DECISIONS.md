# 24 — Current product and protocol decisions

This register contains active decisions only. Superseded decisions and
intermediate architecture history are available in Git.

## Product

| ID | Decision | Status |
|---|---|---|
| DEC-173 | Beta Mail attachments use the shared encrypted Object Storage layer; text-only Mail is the initial integration profile. | Frozen |
| DEC-179 | Mail and Files use familiar Gmail and Google Drive workflows without copying their branding or centralized-provider assumptions. | Frozen |
| DEC-180 | Normal UI presents user actions and outcomes; protocol and cryptographic details use progressive disclosure. | Frozen |
| DEC-181 | A publication is not reported as protected until the required storage durability condition is met. | Frozen |
| DEC-183 | Mail and Files share encrypted objects; ownership and retention references remain application-private. | Frozen |
| DEC-185 | Files is the Beta file-management surface; Backup is post-Beta. | Frozen |
| DEC-191 | Beta onboarding economics are calibrated from measured Mail and Files/Storage use; Mainnet parameters wait for Beta data. | Frozen |
| DEC-194 | Mail, Files, and later Backup are product surfaces over one Identity, finalized state, and shared encrypted content layer. | Frozen |

## Identity and onboarding

| ID | Decision | Status |
|---|---|---|
| DEC-150 | Account creation is permissionless and uses protocol-native anti-Sybil work. | Frozen |
| DEC-151 | Account creation transfers the onboarding bonus from OnboardingPool to System Balance atomically; identity creation does not mint supply. | Frozen |
| DEC-152 | Identity recovery, Identity authorization, PoA finality, Release Signing, and Treasury use separate key roles. | Frozen |
| DEC-165 | AccountID is a random, nonzero, stable 256-bit identifier independent of mnemonic and keys. | Frozen |
| DEC-166 | Recovery requires Ed25519 and ML-DSA-65; key roles derive from the recovery phrase under separate domains. | Frozen |
| DEC-169 | `.cybou` names are 5–32 lowercase ASCII bytes, permanent, and nontransferable in the initial registry. | Frozen |
| DEC-170 | Name claims finalize through commit, work, and reveal. | Frozen |
| DEC-171 | The portable recovery vault is durably saved and reopened before AccountCreate broadcast. | Frozen |
| DEC-193 | Device is not a protocol entity; one account has one current authorization and KEM key set, rotated atomically. | Frozen |

## Protocol reset

| ID | Decision | Status |
|---|---|---|
| DEC-195 | The canonical protocol uses a genesis-bound single-operator hybrid-PQ PoA finalizer, generic RootPublication, and one encrypted content-addressed chunk tree. Full nodes independently validate every block and state transition. No BFT, ValidatorSet, MailTx, compatibility decoder, automatic import, or dual operation path remains in the target. | Frozen target |
| DEC-197 | Encrypted ROOT/INDEX metadata contains ordered ChunkIDs; DATA stores application bytes. ChunkID is full BLAKE3 of stored ciphertext. Providers admit chunks only with finalized-publication Merkle inclusion proofs and enforce their own capacity. | Frozen |
| DEC-198 | Mail has no consensus operation, per-message state object, Mail fee, or Mail quota. Encrypted Mail data and client-owned Inbox/Sent/read indexes are discovered through generic RootPublication. | Frozen |

## Fixed economics

```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
4-unit fees -> 3 Security + 1 Onboarding
```

DEV, Beta, and Mainnet parameters are separate. Priority fees are disabled.
PoT uses deterministic block-height epochs and integer arithmetic; System
Balance is a service budget and does not increase PoT score. No local wall-clock
value affects consensus.

Detailed wire and cryptographic requirements live in the active protocol
documents referenced by `AGENTS.md`.
