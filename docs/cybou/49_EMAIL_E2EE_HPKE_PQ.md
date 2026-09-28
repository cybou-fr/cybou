# 49 — CYBOU Email E2EE: HPKE + post-quantum profile

This document defines the target cryptographic architecture for native CYBOU Email.

It is a protocol design baseline, not a claim of completed audit/certification.

## Standards basis

Target architecture uses:

- HPKE architecture from RFC 9180;
- ML-KEM from NIST FIPS 203;
- DEV-only pinned PQ/T HPKE draft profile for Identity capability publication;
- finalized standards before any Mainnet use.

The DEV-only pinned KEM is:

```text
MLKEM768-X25519 (X-Wing)
```

The DEV HPKE vector suite is base mode with KEM ID `0x647a`, HKDF-SHA256
KDF ID `0x0001`, and ChaCha20Poly1305 AEAD ID `0x0003`, pinned to
`draft-ietf-hpke-pq-05` Appendix A.5 and its exact dependencies in
`89_IDENTITY_KEM_PUBLICATION.md`. This is a DEV implementation target only;
it is not enabled for Mail or Mainnet.

Identity V2 root/device hybrid *signatures* are separate from this mail
*encryption* profile. Device signing keys must never be reused as KEM keys;
recipient-device key authorization and historical sender-key proofs follow
the versioned identity record in `10_IDENTITY_NAMES.md`.

The target recipient key package is published by Identity and is bound to an
authorized device. It contains the approved suite identifier, classical and PQ
KEM public keys, key IDs, and validity/revocation context. Mail must not
maintain a parallel authoritative recipient-key registry. The current
identity record does not publish KEM capabilities, so sending remains
fail-closed; X25519 helper code alone is not a usable hybrid profile.

The finalized package and encapsulation transcript must bind NetworkID,
sender/recipient AccountIDs, sender/recipient device/key IDs, suite ID, message
context, and both classical and PQ encapsulations. A recipient that requires
the hybrid profile must never be silently downgraded. Exact transcript and
wire encodings remain protocol-review gates.

Do not invent a custom hybrid KEM combiner or use independently generated
X25519 and ML-KEM keys as though they were the hybrid profile's keypair.

OpenSSL 3.5 provides FIPS 203 ML-KEM-768 key and encapsulation APIs. That is
implementation support, not a CYBOU wire profile. [RFC 10024](https://www.rfc-editor.org/rfc/rfc10024.html)
standardizes X25519MLKEM768 for the ephemeral TLS 1.3 handshake; its TLS
key-share encoding does not define a persistent Identity recipient package or
a Mail HPKE profile. The IETF [HPKE PQ draft, revision 05](https://datatracker.ietf.org/doc/html/draft-ietf-hpke-pq-05)
specifies the DEV-target hybrid KEM but remains an active Internet-Draft as of
2026-09-28. It normatively pins concrete hybrid KEM draft-03, generic hybrid
KEM draft-12, and HPKE base draft-03. CYBOU pins those revisions for DEV;
they do not update automatically. The DEV package format is frozen in
`89_IDENTITY_KEM_PUBLICATION.md`; Mail remains disabled until its application
transcript, recipient privacy, historical evidence, and integrated cutover
gates pass. No draft-based profile is enabled on Mainnet.
Identity package publication requirements and cutover gates are tracked in
`89_IDENTITY_KEM_PUBLICATION.md`. Do not substitute the TLS group or implement
a local combiner. See
[OpenSSL's KEM API notes](https://docs.openssl.org/3.5/man1/openssl-pkeyutl/).

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

## Beta Object Storage and attachments

The initial DEV/Alpha profile may carry bounded encrypted text in MailTx. Beta
Mail requires CYBOU Store for encrypted attachments and uses this boundary:

```text
ProtectedMail
-> encrypted message and attachment manifest/object(s)
-> content commitment and opaque reference

MailTx
-> content commitment and opaque Store reference
-> key capsules / required crypto metadata
-> sender authentication
```

Attachment bytes never enter the chain or consensus state. The encrypted
manifest contains filenames, MIME types, and object details; Store sees
ciphertext only. The recipient can retrieve after coming online and verifies
the commitment before local decryption. Do not call the Beta Mail path complete
until the Storage durability and retrieval gates in `81_BETA_PRODUCT_SCOPE.md`
pass.


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
