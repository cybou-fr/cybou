# STATUS: HISTORICAL / SUPERSEDED

This audit is retained for project history only. It is not an active v0.0.1 implementation authority.

# 27 — Architecture audit v0.3

This is the second architecture pass over CYBOU before implementation begins.

## Executive conclusion

The project direction is coherent:

```text
Bitcoin-derived C++ full node
    -> isolated CYBOU network
    -> UTXO-derived deterministic state
    -> bounded-history state sync
    -> BFT Proof of Stake
    -> crypto-agile/PQ-capable network
    -> cooperative encrypted storage
    -> Backup / Drive / Messenger / Chat
```

The architecture is now strong enough to begin incremental code work **provided that the first implementation wave is narrow**.

The main risk is no longer conceptual scope. The main risk is allowing inherited Bitcoin assumptions to remain silently authoritative after CYBOU changes the corresponding subsystem.

Therefore documentation migration is a first-class engineering task.

---

## P0 findings

### P0-1 — Bitcoin documentation can reintroduce Bitcoin behavior

A fork can successfully remove Bitcoin network access in code while stale documentation still tells developers/users to:

- run `bitcoin-qt` or `bitcoind`;
- use `.bitcoin`;
- configure `bitcoin.conf`;
- select main/test/signet;
- use Bitcoin DNS seeds;
- call JSON-RPC;
- use mining RPC;
- follow Bitcoin release procedures;
- assume full-history Bitcoin IBD.

This is dangerous because later contributors may faithfully reintroduce behavior the CYBOU design intentionally removed.

**Resolution:** explicit migration classification for every upstream documentation surface.

### P0-2 — Documentation authority must be defined

CYBOU needs a hierarchy:

```text
CYBOU frozen decisions
    >
CYBOU protocol specifications
    >
CYBOU implementation docs
    >
reviewed inherited Bitcoin technical docs
    >
unreviewed Bitcoin upstream docs
```

An unreviewed upstream document is reference material, not a product requirement.

### P0-3 — State sync is consensus architecture, not an optimization

CYBOU's bounded-history model cannot be bolted on after consensus.

State roots, checkpoint certificates, validator-set commitments and snapshot verification must be defined before public BFT formats freeze.

### P0-4 — BFT consensus needs a formal state machine

The selected direction is explicit-finality BFT, but implementation must not start from the shorthand:

```text
proposal -> prevote -> precommit -> 2/3 -> final
```

Locking, round transitions, validator-set updates and crash persistence are consensus-critical.

### P0-5 — Storage 3:1 needs enforceable accounting

The 3:1 rule is viable only if the network counts verified byte-time, not configured disk space, and separately maintains:

- global capacity solvency;
- shard durability;
- placement diversity;
- repair reserve.

### P0-6 — Account disaster recovery remains unsolved

Current goals cannot all be simultaneously true without an additional trust/recovery mechanism:

- user stores no recovery secret;
- provider holds no KMS/recovery authority;
- all devices/keys may be lost;
- encrypted data remains recoverable.

This remains a production blocker, but does not block early devnet work.

---

## P1 findings

### P1-1 — UTXO plus protocol state needs explicit transaction ownership

CYBOU should define which transaction/state-transition family is responsible for:

- value transfer;
- stake;
- validator registration;
- identity/name state;
- storage accounting commitment;
- protocol parameter activation.

Avoid one generic "metadata transaction" with unbounded semantics.

### P1-2 — Configuration must be redesigned

Bitcoin Core exposes a large CLI/config/RPC operational surface.

CYBOU desktop should converge toward:

- safe GUI settings for ordinary users;
- a small set of expert command-line flags;
- no required RPC server;
- no Bitcoin chain-selection flags;
- explicit dev-only flags behind development builds where possible.

### P1-3 — Single process is a product decision

Bitcoin Core contains multiprocess documentation and infrastructure. CYBOU's desktop product deliberately chooses one in-process full node.

Internal interfaces are valuable; process-splitting is not currently a goal.

### P1-4 — Support matrix must be honest

Initial supported platform:

```text
Windows desktop
```

Linux infrastructure/headless builds may be supported for bootstrap/validator operation, but BSD/macOS/Tor/I2P/CJDNS documentation must not imply support unless CYBOU tests it.

### P1-5 — Release security is part of network security

A signed protocol is irrelevant if an attacker can replace `cybou.exe`.

Reproducible builds, release signatures and dependency provenance need to become production requirements.

---

## Architectural invariants after this audit

1. `cybou.exe` is the normal desktop product.
2. Desktop is a full validating node.
3. Desktop has no required REST/JSON-RPC API.
4. Bitcoin network access is impossible in normal CYBOU builds.
5. Bitcoin history is never required for CYBOU.
6. Ordinary CYBOU nodes need current verified state, not all historical block bodies.
7. Archive history is optional.
8. BFT and state-sync formats are designed together.
9. Storage is cooperative and off-chain at shard granularity.
10. 3:1 is a global verified-capacity policy, not triple replication.
11. Data/metadata are encrypted before object storage.
12. Messenger is internal CYBOU messaging, not SMTP.
13. Qt is presentation/system integration; protocol core remains UI-independent.
14. PQ is introduced through crypto-agility, not ad-hoc primitive replacement.
15. Bitcoin documentation survives only by explicit classification.
