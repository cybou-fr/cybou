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

## UX delivery priorities and staged extensions

Beta acceptance first requires reliable familiar Mail/Files/Wallet workflows:
durable local action acknowledgements, recoverable drafts, stable details and
activity, working native drag/drop, attachment reuse, honest evidence and
responsive keyboard/DPI behavior. Existing live integration is not full
acceptance. Home composition may evolve when readability improves.

A bounded Network overview with a schematic France map is a target addition;
it initially shows this node's observed connections and scoped statistics.
The existing Authority page is extended, not recreated. Wider top100/200/300
uptime observation, richer explorer, own-content console and optional test-build
benchmarks are staged extensions with separate data/privacy gates, not assumed
implemented Beta features. Provider placement remains randomized; display
ranking never chooses paid storage providers.

Delivery order: `DESKTOP_UX_DELIVERY_PLAN.md`. Product boundaries:
`NETWORK_AND_ADVANCED_UX.md`. No interface claims universal erasure, verified
independent failure domains or blanket RGPD conformity without evidence.

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

This applies to finalized published semantic data whose encrypted content and
recovery material remain available. Device-local drafts, mailbox organization,
star/offline preferences and console history are not reconstructed merely from
public history and the mnemonic; explain that scope in onboarding and recovery.

Historical KEM recovery after Identity rotation uses the protected
RecoveryBridge flow.

Large file/attachment DATA may be fetched on demand after metadata/index
reconstruction.

## Anti-abuse

There is no AUTH, reputation score or Validation (DEC-284). Abuse is priced by
protocol fees and storage rent in CYBOU and by a flat relay proof-of-work.
