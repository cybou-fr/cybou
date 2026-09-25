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
- Recovery Root requires Ed25519 AND ML-DSA-65; device authorization requires Ed25519 AND ML-DSA-44.
- Portable CYBV2 vault must be durably saved before AccountCreate broadcast. A 24-word recovery path and clean-machine restore are required.
- `.cybou` labels follow the 5–32 ASCII rule and finalized commit/work/reveal. No transfer, expiry, or recycling in the initial registry.
- Keep mail encryption keys separate from identity signing keys; no custom cryptographic primitives.
- Do not reset DEV until the PQ consensus format and names integrate together.
- Cut over DEV directly after the integration gate; discard obsolete DEV state and vaults. Do not build runtime compatibility, automatic import, or a dual operation decoder.
- Keep one canonical implementation and unversioned source/API names for state, operations, and identity. Version bytes belong inside the wire and vault formats only.
- Every production signature path, including user, validator, operator, release, and treasury operations, must follow the PQ key policy; no classical-only fallback.

## Hard rules

### Mail
- MailTx is a first-class CYBOU operation.
- Do not encode mail as an arbitrary script payload.
- The initial Mail profile is one-recipient, text-only, no attachments.
- no permanent per-mail consensus-state object.
- current state contains validation-relevant counters/roots only.
- local client owns Inbox/Sent/read-state indexes.
- strict maximum MailTx size required.
- deterministic size-aware fee required.
- priority fee disabled.

### Pre-Store
- desktop nodes may prune;
- active validators retain required canonical pre-Store MailTx history;
- mass-scale Email waits for Object Storage.

### BFT
- BFT explicit finality;
- operator-approved admission;
- equal validator weight = 1;
- 4 validators minimum when claiming f=1 tolerance.

### PoT
- block-height-derived deterministic epoch;
- integer arithmetic only;
- System Balance is service budget only (does not boost Beta PoT score);
- PoT in Beta is account age and protocol history only;
- deterministic fixed mail quota per epoch;
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

### Operator keys
Keep separate:
- Operator Authority;
- Operator Validator;
- Release Signing;
- Treasury.

### Evidence
Mail evidence must support:
- transaction inclusion proof;
- BFT finality certificate;
- historical sender-key authorization;
- salted/domain-separated content commitment.

### Economics
```text
MAX_SUPPLY = 100,000,000,000
decimals = 0
4 fees -> 3 Security + 1 Onboarding
```
