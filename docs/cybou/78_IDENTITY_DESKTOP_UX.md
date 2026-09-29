# 78 — Identity desktop UX contract

Status: Identity creation, clean-machine restore, and all-role key rotation are the desktop workflows. The UI must not expose device registration or a per-device security model.

The first screen gives equal prominence to **Create identity** and **Restore identity**. Finalized facts come from native core. Local phrase, vault, and work states must never imply consensus finality.

## Create

1. Set a vault password, generate 24 words locally, and confirm selected words without placing them in logs or clipboard by default.
2. Generate a random AccountID and derive Recovery, Authorization, and X-Wing key roles from the entropy using separate domains.
3. Atomically save and reopen the CVID5 vault before AccountCreate broadcast.
4. Perform AccountCreationWork, submit, and show pending until verified PoA finality. Keep the vault after network failure so the same identity can retry.
5. Offer a `.cybou` name. Persist claim salt in the encrypted local claim file before NameCommit; show commit, work, reveal, and finality as separate phases.

The active header shows a finalized primary name, when present; AccountID is a secondary copyable technical identifier. An unfinalized name is never shown as owned.

## Restore and security

Restore accepts 24 words on a clean machine, derives the key roles, locates AccountID by RecoveryKeyID in verified state, checks the current authorization and KEM commitment/key_epoch, and saves a local vault under a new password. Restore performs no consensus mutation. A phrase from a prior key_epoch is not accepted as the current identity.

Identity rotation confirms a new 24-word phrase, saves and reopens a candidate vault, journals exact IdentityRotate bytes, and keeps the current vault until verified finality. An interrupted rotation resumes from the encrypted candidate and exact journal. Phrase viewing remains behind reauthentication. Distinguish local errors, network rejection, and pending finality without leaking secrets.

Acceptance requires interrupted-creation recovery, clean-machine restore, wrong-password/tamper rejection, exact retry after uncertain delivery, and continuity of AccountID, name, and balances through IdentityRotate.
