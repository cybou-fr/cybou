# 76 — Identity vault and recovery

## Recovery phrase

The current 24-word mnemonic encodes the recovery entropy from which current
Recovery, Authorization and X-Wing KEM roles are derived under separate domains.

AccountID remains stable across Identity rotation.

## Portable vault

CYBV2/CVID5 stores stable AccountID plus recovery entropy inside the protected
portable vault format.

The vault must be durably saved and reopened before AccountCreate broadcast.

## Basic clean restore

From the current mnemonic:

1. validate mnemonic;
2. derive current Identity roles;
3. resolve AccountID from verified finalized state;
4. verify current Recovery/Authorization/KEM commitment against current
   `key_epoch`;
5. create/reopen the local vault;
6. read canonical Balance, System Balance and AUTH from finalized AccountState;
7. rebuild private Mail/Files through publication discovery.

Restore itself does not authorize a new protocol operation.

## Historical content after rotation

Old RootPublications may have recipient/self capsules addressed to old KEM
epochs. A new mnemonic does not automatically derive those old KEM seeds.

Therefore rotation that must preserve historical content recovery uses a
private `IDENTITY_RECOVERY_BRIDGE`.

Before submitting IdentityRotate:

```text
generate/confirm new mnemonic
-> derive new KEM
-> build encrypted RecoveryBridge containing required old KEM recovery material
-> capsule bridge to the new KEM
-> publish with current old Identity authorization
-> obtain PoA finality
-> reach remote durability
-> verify bridge can be opened using the new mnemonic
-> only then submit IdentityRotate
```

Recovered historical KEM seeds are accepted only after deriving their public
packages and matching the canonical historical KEM commitments.

Do not combine an unprotected bridge and irreversible rotation into one step.

## Local application state

The per-Identity Application DB and previous provider placement database are
rebuildable caches and are not required inputs for clean recovery.
