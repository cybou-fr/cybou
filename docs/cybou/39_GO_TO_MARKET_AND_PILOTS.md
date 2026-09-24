# 39 — Go-to-market and pilot strategy

## Category

First product:

```text
European sovereign identity and communication network

CYBOU Email
```

Native E2E encrypted consensus-registered email built on user-controlled
network identity.

## First adoption unit

An organization/team with an existing contact graph.

Do not optimize the first launch around isolated individual users. A team
adopts CYBOU together so users have people to contact on day one.

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
European sovereign identity and communication
user-controlled network identity
E2E encrypted email
network-native .cybou identity
consensus registration/finality
cryptographic proof of origin/integrity
recipient can be offline
client-controlled keys
```

Do not lead with coin/staking/UTXO mechanics.

## Expectation management

```text
CYBOU Email v1 is CYBOU-native.
It is not an SMTP/IMAP replacement gateway yet.
It is text-only before Object Storage.
```

## Pilot metrics

Canonical user journey:

```text
install CYBOU
	-> create stanislav.cybou
	-> secure recovery
	-> send encrypted mail to alice.cybou
	-> Alice is offline
	-> Alice opens CYBOU later and sees verified mail
	-> Alice replies
	-> both restart and retain correct identity/mail state
```

This journey is an architecture acceptance test, not only a marketing story.

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

Pilot success is repeated exchange of mail, recovery, multi-device use and
continued usage, not registration count alone.
