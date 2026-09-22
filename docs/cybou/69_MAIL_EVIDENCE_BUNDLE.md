# 69 — Mail evidence bundle

CYBOU Email should support exportable cryptographic evidence for a finalized MailTx.

## EvidenceBundle

Conceptually:

```text
MailEvidenceBundle {
    mail_tx
    block_header
    transaction_inclusion_proof
    finality_certificate
    sender_key_state_proof
    protocol_version
}
```

## Sender key at historical height

It is not enough to show that a signing key belongs to an AccountID today.

The evidence must prove that the sender signing key was authorized for that AccountID at the MailTx finalization height.

This preserves verification after later key rotation or revocation.

## Content commitment

Do not publish a simple hash of predictable plaintext.

Preferred pattern:

```text
salt = cryptographically random

ContentCommitment =
    H(
      "CYBOU-MAIL-CONTENT-V1"
      || salt
      || canonical_plaintext_mail
    )
```

The salt remains inside E2E encrypted mail content unless the recipient later chooses to disclose it.

## Why salted commitment

For short/predictable plaintext, a bare plaintext hash can permit dictionary guessing.

A random salt prevents practical offline guessing before voluntary disclosure.

## Later disclosure

If a recipient wants to prove content externally:

```text
plaintext
+ salt
+ MailEvidenceBundle
```

can be checked against:

```text
ContentCommitment
sender signature
block inclusion
BFT finality
historical sender-key authorization
```

## Time semantics

Evidence should distinguish:

```text
client_created_time
consensus timestamp if defined
finalized block height
```

The strongest protocol fact is finalized ordering/height.

Do not claim exact legal trusted time unless the consensus timestamp rules and applicable legal qualification support that claim.
