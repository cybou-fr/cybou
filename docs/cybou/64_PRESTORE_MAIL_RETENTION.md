# 64 — Pre-Store Mail retention contract

Before CYBOU Object Storage exists, encrypted MailTx ciphertext lives in block/history data.

Therefore CYBOU needs an explicit temporary retention contract.

## Roles

### Desktop Full Node

May use bounded-history pruning.

It is not required to retain all historical MailTx bodies forever.

### Validator

Before Store becomes production-ready, every active validator MUST retain the full canonical pre-Store MailTx block history required to serve historical native Email content.

This is a temporary architecture rule.

### Archive Node

May retain full historical blocks independently of validator status.

## Why validators retain pre-Store mail

A recipient can be offline for a long period.

Without Store, if every node prunes the only ciphertext copy:

```text
MailTx proof remains
but encrypted body is unrecoverable
```

Validator archival retention avoids that failure during the controlled pre-Store phase.

## Scale gate

Pre-Store Email is suitable for:

```text
development
controlled alpha/beta
French organizational pilot
small controlled production
```

It is NOT the architecture for unrestricted mass-scale Email.

Before broad-scale growth:

```text
CYBOU Object Storage must be production-ready
```

or an equivalent durable encrypted content layer must exist.

## Retention transition to Store

Once Store is activated by a protocol version:

```text
new MailTx
-> content root / Store object reference
```

At that point validators no longer need to retain all future mail bodies in block history.

Historical pre-Store MailTx remains governed by the pre-Store archival rule or a separately defined migration process.

## No false durability claim

Before Store, product copy must not promise permanent independent recovery of old mail solely from current state.

The protocol guarantees registration/finality.

Content durability is governed by this retention contract.
