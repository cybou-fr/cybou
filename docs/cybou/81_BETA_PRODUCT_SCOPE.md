# 81 — Beta product scope

CYBOU Beta is an Identity-centered desktop product with:

```text
Identity
Names
Wallet
Mail
Files
```

Backup remains post-Beta.

## Mail

Beta Mail provides familiar desktop Mail workflows over private
RootPublications.

Target includes:

- one-recipient protected Mail;
- Inbox/Sent/Drafts/local organization;
- attachments over the shared encrypted content tree;
- local/private search;
- offline recipient recovery;
- clean-machine reconstruction.

## Files

Beta Files provides familiar Drive-like workflows:

- files/folders;
- upload/download;
- rename/move/copy;
- trash/restore/delete semantics;
- private version/content references;
- Mail attachment reuse;
- local/private search.

## Common lifecycle

Default finality-first lifecycle:

```text
prepare/encrypt locally
-> RootPublication
-> PoA finality
-> remote provider admission
-> durability target (2 independent remote replicas)
-> Sent / Protected
```

Validation signatures never trigger remote placement.
Remote durability (`Protected`) always requires verified PoA finality.

## Beta durability

For every required chunk:

```text
2 independent remote full replicas
```

are required for Beta `Protected`.

The local encrypted copy is useful cache/offline state but does not count as
one of the two remote replicas; with it Beta normally keeps three physical
copies.

Development uses one remote replica before the Beta gate.

Beta does not use Reed-Solomon/erasure coding.

## Local storage model

The physical ChunkStore contains encrypted bytes only and is not a user file
browser.

Each unlocked Identity has a separate encrypted rebuildable Application DB for
Mail/Files semantic state.

The GUI never exposes arbitrary foreign provider chunks.

## Clean recovery

A clean machine with only the current mnemonic and public network data must be
able to rebuild:

```text
Identity
Names
Wallet
Mail
Files
```

without the old Application DB.

Historical KEM recovery after Identity rotation uses the protected
RecoveryBridge flow.

Large file/attachment DATA may be fetched on demand after metadata/index
reconstruction.

## Authority

AUTH is a canonical network value attached to the Identity, read from the
latest finalized state. Its sole protocol effect is qualifying an Identity to
sign Validation when `Authority > 1,000,000 AUTH`. Validation is shown as
additional pre-finalization confidence, never as a quorum or BFT guarantee.
It does not allocate resources,
confer stake weight, or grant PoA finalization power, and is not a gamified social score.
