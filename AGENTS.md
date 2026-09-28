# AGENTS.md — CYBOU v0.0.1 implementation authority

Read active docs before coding.

### DEV VPS deployment
- After changing CYBOU core or `cybou-node`, rebuild `cybou-node` on the DEV VPS and restart `cybou-node.service` in the same task.
- Connect as `debian@vps-d0669a91.vps.ovh.net` over SSH. The checkout is `/home/debian/cybou`; the service runs `/home/debian/cybou/build/bin/cybou-node`.
- Run relevant tests before deployment. Preserve the current executable for rollback, build the updated executable on the VPS, restart with systemd, then verify service health, listening port, and advancing block height.
- Do not reset DEV state or replace the validator key as part of a routine deployment.

### Identity target
- Read `docs/cybou/10_IDENTITY_NAMES.md` and `76`–`78` before Identity work.
- Random stable AccountID is independent of mnemonic and keys.
- Recovery signing requires Ed25519 AND ML-DSA-65; Identity authorization requires Ed25519 AND ML-DSA-44. All current Identity signing and KEM roles derive from mnemonic entropy under separate domains.
- Portable CYBV2/CVID5 vault stores stable AccountID plus recovery entropy; all Identity roles are derived from entropy. Durably save and reopen before AccountCreate. Clean-machine restore verifies every current key role and KEM commitment against finalized key_epoch and does not create a protocol authorization operation.
- `.cybou` labels follow the 5–32 ASCII rule and finalized commit/work/reveal. No transfer, expiry, or recycling in the initial registry.
- Keep mail encryption keys separate from Identity signing keys; no custom cryptographic primitives. IdentityRecord has recovery key, authorization key, current KEM commitment, one shared nonce, and key_epoch. IdentityRotate atomically replaces all roles; no device registry, activation, per-device nonce, DeviceAdd, or DeviceRevoke.
- Current deployed DEV remains on the existing BFT/MailTx/Object protocol until one complete PoA + RootPublication + encrypted chunk-DAG cutover passes its integration gate.
- The target protocol is a genesis-bound single-operator PoA finalizer with independently validating full nodes. This is centralized PoA, not BFT and not Byzantine-fault-tolerant finality.
- PoA signing is a separate mnemonic-derived role, held in memory only, and requires Ed25519 AND ML-DSA-65. Identity authorization/recovery, PoA, Release Signing, and Treasury keys remain separate; no classical-only production signature path.
- Keep the PoA anti-equivocation journal durable and fail closed. Deterministic fork/conflicting-signature and journal-rollback handling are cutover gates.
- Cut over DEV directly after all format, names, finality, state-execution, chunk-admission, and clean-machine recovery gates pass; discard obsolete DEV state and vaults. Do not build runtime compatibility, automatic import, or a dual operation decoder.
- Keep one canonical implementation and unversioned source/API names for state, operations, and identity. Version bytes belong inside the wire and vault formats only.
- Every production signature path, including user, validator, operator, release, and treasury operations, must follow the PQ key policy; no classical-only fallback.

## Hard rules

### Protocol target: RootPublication + encrypted chunk DAG
- Read `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md` before protocol implementation.
- After coordinated cutover, generic RootPublication is the only application-content publication operation. Mail, Files, Backup, filenames, recipients, graph structure, and application schemas are encrypted payload data, not consensus operation types.
- ChunkID is the full 256-bit BLAKE3 of stored encrypted bytes. Use a vetted BLAKE3 implementation; do not implement primitives locally. Payload nodes use a bounded canonical-CBOR profile and reviewed AEAD/KDF/KEM.
- Unfinalized operations and chunks remain local. A finalized RootPublication authorizes chunk admission; it does not prove durability. The client retains ciphertext and reports availability only after the frozen durability threshold.
- Providers accept only content-addressed chunks with a valid finalized-publication Merkle admission proof. They store opaque ciphertext and required proof/lease metadata.
- Recipient capsules do not expose AccountID. Local clients scan finalized publications and rebuild service indexes; clean-machine recovery must work from mnemonic plus public network data.
- Keep Mail and Files UX requirements. Initial DEV/Alpha product UX remains one-recipient text-only until the new protocol reaches its own integration gate; Beta attachments use the shared chunk DAG.
- Consensus may enforce generic publication byte/count limits and deterministic fees, but cannot enforce hidden Mail-specific quotas.

### Historical protocol
- Existing DEV/BFT/MailTx/Object Storage documents describe the currently running protocol only unless explicitly marked as the post-cutover target.
- Do not mix old BFT/MailTx or indexed-manifest formats into the new genesis or implement dual decoders.
- Beta durability, pruning, retention, repair, and economics remain explicit readiness gates for the new ChunkStore.

### PoT
- block-height-derived deterministic epoch;
- integer arithmetic only;
- System Balance is service budget only (does not boost Beta PoT score);
- PoT in Beta is account age and protocol history only;
- deterministic generic publication byte/count limits; no consensus Mail quota once application type is encrypted;
- no local wall-clock consensus logic.

### Onboarding & Anti-Sybil
- No Voucher architecture.
- No Operator-authorized ordinary user onboarding.
- No central account activation.
- Permissionless protocol-native AccountCreateOp with AccountCreationWork.
- Account creation requires protocol anti-Sybil validation.
- Successful AccountCreate atomically funds SystemBalance from OnboardingPool.
- No service-specific free-credit systems (services consume SystemBalance).
- DEV, Beta, and Mainnet economic parameters are separate.
- Beta and Mainnet have separate genesis (Beta balances do not carry to Mainnet).
- Beta onboarding budget is determined using integrated Email + Storage + Backup economics.
- Mainnet onboarding bonus is frozen only after aggregate Beta operational data.

### Keys
Keep Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing, and Treasury roles separate.

### Evidence
Publication evidence must support:
- RootPublication inclusion proof;
- hybrid PoA finality proof;
- historical sender Identity authorization;
- domain-separated chunk commitment and admission proof.

### Economics
```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
4 fees -> 3 Security + 1 Onboarding
```
