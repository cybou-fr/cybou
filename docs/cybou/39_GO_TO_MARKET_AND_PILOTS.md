# 39 — Go-to-market and pilot strategy

## Category

First product:

```text
CYBOU Email
```

Native E2E encrypted consensus-registered email.

## First adoption unit

An organization/team with an existing contact graph.

Recommended first pilot:

```text
20–100 users
French organization
small controlled traffic volume
4 approved validators where f=1 BFT tolerance is claimed
8–12 weeks
controlled support
```

## Pitch

Lead with:

```text
European sovereign E2E encrypted email
network-native .cybou identity
consensus registration/finality
cryptographic proof of origin/integrity
recipient can be offline
client-controlled keys
PQ/T migration path
```

Do not lead with coin/staking/UTXO mechanics.

## Expectation management

```text
CYBOU Email v1 is CYBOU-native.
It is not an SMTP/IMAP replacement gateway yet.
It is text-only before Object Storage.
```

## Pilot metrics

- MailTx submit-to-finality latency;
- successful later synchronization by recipients who were offline;
- MailTx discovery/filter efficiency;
- decrypt/authentication failures;
- recipient-key/package failures;
- real serialized MailTx size;
- blockchain/history growth;
- validator disk growth;
- state size growth;
- client CPU/RAM/network use;
- device revocation success;
- support burden;
- user retention.
