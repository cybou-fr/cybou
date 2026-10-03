# 39 — Go-to-market and pilot strategy

## Category

First product:

```text
European sovereign identity and communication network

CYBOU Email
```

End-to-end encrypted Mail and private Files built on user-controlled network
Identity and one shared encrypted content layer.

## First adoption unit

An organization/team with an existing contact graph.

Do not optimize the first launch around isolated individual users. A team
adopts CYBOU together so users have people to contact on day one.

Recommended first pilot:

```text
20–100 users
French organization
small controlled traffic volume
single-operator PoA finality disclosed in pilot materials
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
finalized publication with cryptographic inclusion and sender authorization
proofs
recipient can be offline
client-controlled keys
familiar Mail and Files workflows without a central provider
hybrid-PQ end-to-end protection
client-encrypted distributed attachment/file storage
```

Do not lead with coin/staking/UTXO mechanics.

## Expectation management

```text
CYBOU Email is CYBOU-native.
It is not an SMTP/IMAP replacement gateway yet.
The initial DEV/Alpha profile is text-only. Beta Mail includes encrypted
attachments backed by CYBOU Object Storage; do not pitch text-only mail as the
complete Beta product.
```

## Pilot metrics

Canonical user journey:

```text
install CYBOU
	-> create stanislav.cybou
	-> secure recovery
	-> send encrypted mail to alice.cybou
	-> attach an encrypted PDF or photo
	-> Alice is offline
	-> Alice opens CYBOU later, verifies the mail, retrieves and decrypts the attachment
	-> Alice replies
	-> both restart and retain correct identity/mail state
```

This journey is an architecture acceptance test, not only a marketing story.

- RootPublication submit-to-finality latency;
- successful later synchronization by recipients who were offline;
- publication discovery and private-index rebuild efficiency;
- decrypt/authentication failures;
- recipient-key/package failures;
- serialized RootPublication size and chunk count;
- blockchain/history growth;
- provider disk growth and durability;
- state size growth;
- client CPU/RAM/network use;
- Identity key rotation success;
- support burden;
- user retention.
- task-completion success for Compose, attachment send, offline receive,
  Save to Files, file upload/download, and recovery without operator guidance;
- UI responsiveness during network, cryptographic, and Storage work;
- share of pilot users who complete the end-to-end Mail/Files flow without
  protocol education.

Pilot success is repeated exchange of mail, recovery, clean-machine recovery and
continued usage, not registration count alone.
