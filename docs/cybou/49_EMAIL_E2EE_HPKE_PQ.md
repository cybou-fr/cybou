# 49 — CYBOU Email E2EE: HPKE + post-quantum profile

This document defines the target cryptographic architecture for native CYBOU Email.

It is a protocol design baseline, not a claim of completed audit/certification.

## Standards basis

Target architecture uses:

- HPKE architecture from RFC 9180;
- ML-KEM from NIST FIPS 203;
- standardized/finalized PQ/T HPKE construction when available.

Preferred target remains:

```text
X25519 + ML-KEM-768
```

Do not invent a custom hybrid KEM combiner.

## v1 MailTx encryption

Before Object Storage:

```text
plaintext text email
        ↓
random CEK
        ↓
AEAD encrypt subject + body once
        ↓
HPKE wrap CEK for authorized recipient device(s)
        ↓
sender signs protected mail commitment
        ↓
MailTx
        ↓
BFT consensus
```

The chain contains ciphertext, never plaintext.

## Protected mail

Conceptually:

```text
ProtectedMail {
    protocol_version
    mail_id
    thread_id
    sender_account_id
    client_created_time
    subject
    text_body
    recipient_semantics
}
```

This object is E2E encrypted.

## MailTx outer structure

Conceptually:

```text
MailTx {
    version
    mail_id
    opaque_recipient_tag
    crypto_suite
    recipient_device_key_id_or_set
    hpke_encapsulation_or_key_capsules
    ciphertext
    ciphertext_hash
    sender_authentication
    fee
}
```

Only fields necessary for consensus, recipient discovery and verification remain outside ciphertext.

Minimize public metadata.

## Sender authentication

HPKE confidentiality and sender identity authentication are separate.

The sender signs the protected mail commitment with an AccountID/device-authorized signature profile.

PQ-capable target may use an ML-DSA-family signature or reviewed hybrid transition profile.

Exact signature profile remains a separate freeze point.

## Multi-device

Mail content is encrypted once with one CEK.

The CEK is independently wrapped to authorized recipient devices.

Revoked devices are excluded from future mail.

## Recipient privacy

A public chain can accidentally expose a social graph.

Therefore raw human-readable `.cybou` recipient names should not be placed in every MailTx.

The protocol should use an opaque/derived recipient discovery tag or another reviewed mechanism so recipient clients can efficiently identify relevant mail while leaking as little relationship metadata as practical.

Exact mechanism remains open.

## Associated data

Bind protocol-critical context, such as:

```text
CYBOU Mail domain separator
protocol version
crypto-suite version
MailID
recipient device/key identifier
```

Do not expose subject/body through associated data.

## Device key packages

Each authorized device publishes an authenticated encryption key package.

Private keys and CEKs never go on chain.

## Forward-secrecy caveat

Static HPKE receiver keys alone do not imply Signal-style post-compromise security.

Do not claim forward secrecy or post-compromise security until the exact rotating/prekey design exists and is reviewed.

## Future Object Storage

When CYBOU Store exists:

```text
ProtectedMail
-> encrypted object / manifest
-> content_root

MailTx
-> content_root
-> key capsules / required crypto metadata
-> sender authentication
```

The same E2E rule remains: Store sees ciphertext only.


## Content commitment

MailTx should include a cryptographic commitment to canonical plaintext mail content without exposing a guessable bare plaintext hash.

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

The salt remains inside E2E encrypted content unless the recipient later chooses to disclose it.

This supports external evidence export while reducing dictionary-guessing risk.

## Historical sender-key proof

An exported evidence bundle must prove that the sender signing key was authorized for the AccountID at the historical finalization height, not only at the present time.
