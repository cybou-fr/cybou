# 78 — Identity desktop UX contract

Status: target UX. The current IdentityPage has not completed the PQ cutover.

The first screen gives equal prominence to **Create identity** and **Restore
identity**. All finalized facts come from native core. Local phrase, vault,
and work states must never imply consensus finality.

## Create

1. Set a vault password, generate 24 words locally, and confirm selected
   words without placing them in logs or clipboard by default.
2. Generate random AccountID and root/device keys. Atomically save and reopen
   the encrypted vault **before** AccountCreate broadcast.
3. Perform account work, submit, and show pending until verified BFT finality.
   Keep the vault after network failure so the same identity can retry.
4. Offer a `.cybou` name. Persist claim salt before NameCommit; show commit,
   work, reveal, and finality as separate phases.

The active header shows a finalized primary name, when present; AccountID is
a secondary copyable technical identifier. An unfinalized name is never shown
as owned.

## Restore and security

Restore accepts 24 words on a clean machine, derives RecoveryKeyID, locates
AccountID in verified state, authorizes a new device, and saves a new vault
under a new password. Show waiting for sync when state is unavailable.
Security exposes backup status, vault lock, password change, phrase viewing
behind reauthentication, and encrypted export. Devices lists active and
revoked keys with finality; Name shows claim state. Distinguish local errors,
network rejection, and pending finality without leaking secrets.

Acceptance requires interrupted-creation recovery, clean-machine restore,
wrong-password/tamper rejection, independent device nonces, and continuity
of AccountID, name, and balances through recovery rotation.
