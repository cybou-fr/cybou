# 62 — Mail registration and proof model

CYBOU Email's distinguishing property is consensus registration.

## Registration chain

```text
canonical plaintext text
    ↓
random salt + ContentCommitment
    ↓
E2E encryption
    ↓
ciphertext / CiphertextHash
    ↓
sender signature
    ↓
first-class MailTx
    ↓
block inclusion
    ↓
BFT finality
```

No permanent per-mail current-state object is required for this proof.

## Evidence

A verifier can prove protocol facts such as:

```text
MailTx canonical bytes/hash
sender signature validity
historical sender-key authorization
block inclusion
finalized block height
finality certificate
ContentCommitment
CiphertextHash
```

## Export bundle

Use `MailEvidenceBundle` defined in `69_MAIL_EVIDENCE_BUNDLE.md`.

## Time semantics

Distinguish:

```text
client_created_time
consensus/block timestamp if formally defined
finalized block height
```

Finalized ordering/height is the strongest base protocol fact.

## Recipient acknowledgement

v1 does not require a read receipt.

A future optional `MailAckTx` may explicitly state a recipient acknowledgement.

Do not equate synchronization/opening with legal acceptance.

## Legal boundary

CYBOU provides cryptographic registration evidence.

Do not market it as a legal notarization service or legally qualified registered-delivery service without jurisdiction-specific review.
