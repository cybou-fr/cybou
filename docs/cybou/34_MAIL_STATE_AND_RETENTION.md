# 34 — Mail state and retention

CYBOU v0.14 removes the design where every email creates a permanent per-mail object in current consensus state.

## Core rule

```text
MailTx belongs to block/history data.

Current consensus state must remain bounded
and must NOT contain one permanent MailMarker per email.
```

The chain itself, together with block commitments and BFT finality, proves that a MailTx existed.

## v1 before Object Storage

```text
Block/history data:
    full encrypted text MailTx

Current consensus state:
    account counters / limits
    balances
    identities
    validators
    Proof of Trust state
    compact block-level mail discovery commitments/filters if needed

Local client state:
    Inbox/Sent/Archive index
    decrypted local mailbox
```

## Why permanent MailMarker-per-mail is rejected

A permanent object for every email would make current state grow forever in direct proportion to email volume.

That defeats bounded-state goals even if old block bodies are pruned.

Therefore v0.14 freezes:

```text
NO permanent MailMarker[MailID] for every email.
```

## Mail discovery

The preferred direction is block-level or range-level discovery metadata rather than consensus state per message.

Candidate:

```text
compact recipient-tag filter per block/range
```

A recipient can test whether a block/range may contain relevant MailTx before fetching/processing the full block body.

The exact filter construction is still open, but it must not create permanent per-email current state.

## Local mailbox index

After a recipient verifies/decrypts mail, the client stores a local index:

```text
LocalMailIndex {
    mail_id
    block_height
    tx_position_or_txid
    sender
    local_folder
    read_state
    local_cached_ciphertext_or_plaintext_policy
}
```

This index is application data, not consensus state.

## Proof after pruning

A MailTx inclusion/finality proof can establish that a mail record existed.

However:

```text
proof/commitment != content storage
```

If ciphertext bytes are unavailable, they cannot be reconstructed from a hash.

That is why pre-Store retention is defined separately.

## Future Object Storage

After Store:

```text
Blockchain:
    MailTx with content commitment / object reference

Current state:
    no permanent per-mail object requirement

CYBOU Store:
    encrypted body + attachments

Local client:
    mailbox index/cache
```

This is the long-term scalable architecture.
