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

The DEV cryptographic profile is frozen to base mode with KEM ID `0x647a`,
HKDF-SHA256 KDF ID `0x0001`, and ChaCha20Poly1305 AEAD ID `0x0003`, pinned to
`draft-ietf-hpke-pq-05` Appendix A.5 and its exact dependencies in
`89_IDENTITY_KEM_PUBLICATION.md`. This is an implementation target for DEV
only; it is not enabled for Mail or Mainnet.

Identity V2 root/device hybrid *signatures* are separate from this mail
*encryption* profile. Device signing keys must never be reused as KEM keys;
recipient-device key authorization and historical sender-key proofs follow
the versioned identity record in `10_IDENTITY_NAMES.md`.

The DEV Identity record publishes a draft-05 X-Wing package for each active
authorized device. The package is bound to AccountID and device activation;
Mail must not maintain a parallel authoritative recipient-key registry. The
application envelope and recipient-set profile are frozen below, but
`SendMail` remains fail-closed until an approved backend supports the pinned
X-Wing HPKE suite and the integration is complete. The former classical
X25519-only helper has been removed.

The DEV cryptographic parameters for Mail are fixed as follows:

| Purpose | Frozen DEV choice |
|---|---|
| Recipient account count | One |
| Recipient device coverage | One CEK capsule for every active authorized recipient device, ordered by DeviceKeyID |
| CEK | Fresh random 32 bytes per message |
| Content encryption | ChaCha20-Poly1305, fresh random 12-byte nonce, encrypt the protected text once |
| CEK wrapping | HPKE base mode, one context per recipient device, wrapping only the 32-byte CEK |
| HPKE suite | X-Wing KEM `0x647a` + HKDF-SHA256 `0x0001` + ChaCha20-Poly1305 `0x0003` |
| Sender authorization | Existing device hybrid signature (Ed25519 + ML-DSA-44) over the canonical Mail payload commitment |
| Content commitment | SHA-256 over ASCII `CYBOU/MAIL_COMMIT/V2` || 32-byte random salt || canonical protected plaintext; salt stays encrypted |
| Mainnet | Disabled until final standards and a new Mainnet profile review |

The DEV Mail envelope is frozen below. Its recipient AccountID is public in
the current MailTx and the discovery tag is a deterministic filter key, so DEV
does not claim recipient-relationship privacy. Mainnet requires a separate
privacy review and profile. `SendMail` and incoming decryption remain disabled
until the frozen envelope is implemented with a backend that supports the
selected X-Wing HPKE suite; incoming decryption also requires historical
sender-key evidence verification.

The HPKE context binds NetworkID, sender/recipient AccountIDs, sender and
recipient DeviceKeyIDs and activation nonces, recipient package commitment,
recipient state height/root, MailID, content commitment, and suite tuple. A
recipient that requires the hybrid profile must never be silently downgraded.
Unknown suite or envelope versions are rejected.

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

## DEV Mail envelope v1

The envelope is carried in the existing `MailPayload.ciphertext` field. All
multi-byte integers in this envelope are unsigned little-endian unless stated
otherwise. The 16-bit HPKE suite identifiers are serialized as three unsigned
big-endian values in KEM, KDF, AEAD order.

```text
MailEnvelopeV1 {
    envelope_version: u8 = 1
    kem_id: u16be = 0x647a
    kdf_id: u16be = 0x0001
    aead_id: u16be = 0x0003
    mail_id: 32 random bytes
    recipient_state_height: u64le
    recipient_state_root: 32 bytes
    content_nonce: 12 random bytes
    capsule_count: u8                 // 1..8
    capsules[capsule_count]           // ascending DeviceKeyID
    content_ciphertext_length: u32le  // includes 16-byte AEAD tag
    content_ciphertext: bytes
}

RecipientCapsuleV1 {
    recipient_device_key_id: 32 bytes
    recipient_activation_nonce: u64le
    recipient_package_commitment: 32 bytes
    hpke_enc: 1120 bytes              // X-Wing draft-05
    wrapped_cek: 48 bytes             // 32-byte CEK + 16-byte tag
}
```

`capsule_count` must equal the active-device count in the sender's finalized
recipient snapshot. Each capsule uses a fresh HPKE base-mode context and wraps
only the same 32-byte message CEK. The receiver resolves the device package
historically using the encoded activation nonce and verifies its commitment
against the encoded finalized state snapshot. Duplicate or unsorted device
IDs, an incorrect count, a mismatched package, trailing bytes, and unknown
versions/suites are rejected. No downgrade or fallback is permitted.

The HPKE `info` is the following concatenation, with no terminators or length
prefixes: ASCII `CYBOU/MAIL/CEK-WRAP/V1`; `mail_id`; the three suite IDs as
u16be; NetworkID; sender AccountID; sender DeviceKeyID; sender activation
nonce u64le; recipient AccountID; recipient DeviceKeyID; recipient activation
nonce u64le; recipient package commitment; recipient state height u64le;
recipient state root; and content commitment. HPKE AAD is the empty byte
string. Sender identity authentication remains the outer hybrid device
authorization over the canonical MailPayload commitment.

Content AEAD AAD is the concatenation, with no terminators or length prefixes:
ASCII `CYBOU/MAIL/CONTENT/V1`; envelope version u8; the three suite IDs as
u16be; NetworkID; MailID; sender AccountID; sender DeviceKeyID; sender
activation nonce u64le; recipient AccountID; recipient state height u64le;
recipient state root; and content commitment.

The protected plaintext is `salt[32] || ProtectedTextV1`; the random salt is
encrypted and is not sent in the clear. `ProtectedTextV1` is:

```text
version: u8 = 1
sender_account_id: 32 bytes
recipient_account_id: 32 bytes
client_created_time: u64le       // informational only; never consensus input
subject_length: u16le             // UTF-8 bytes, maximum 256
subject: subject_length bytes
body_length: u32le                // UTF-8 bytes, maximum 48000
body: body_length bytes
```

The initial profile has no thread ID, HTML, compression, or attachments.
Reject invalid UTF-8, lengths above the limits, and trailing bytes. The
content commitment is SHA-256 over ASCII `CYBOU/MAIL_COMMIT/V2`, the 32-byte
salt, and the exact `ProtectedTextV1` bytes.

The public discovery tag is SHA-256 over ASCII
`CYBOU/MAIL/DISCOVERY/V1 || NetworkID || recipient AccountID`; it is a filter
key, not a privacy mechanism. The recipient AccountID remains visible in the
outer MailPayload. DEV accepts this metadata leakage; Mainnet must not inherit
that choice without a separate privacy review.

The inner envelope is capped at 65,536 bytes. Including the fixed 2,698-byte
authorized-mail header/payload prefix, `SerializeAuthorizedMail` is capped at
68,234 bytes before the outer operation-type byte. The existing DEV fee is
`4 + ceil(ciphertext_bytes / 1024)` with no priority fee.

## Protected text

Conceptually:

```text
ProtectedTextV1 {
    version
    sender_account_id
    recipient_account_id
    client_created_time
    subject
    text_body
}
```

This object is E2E encrypted.

## MailTx outer structure

Conceptually:

```text
MailTx {
    version
    recipient_account_id
    discovery_tag
    recipient device set and HPKE capsules inside MailEnvelopeV1
    content ciphertext inside MailEnvelopeV1
    hybrid device authorization
    fee
}
```

The DEV outer payload format and inner MailEnvelopeV1 encoding are frozen
above. Recipient AccountID and discovery tag are public; DEV makes no
recipient-relationship privacy claim.

## Sender authentication

HPKE confidentiality and sender identity authentication are separate.

The sender uses the existing AccountID/device authorization: Ed25519 and
ML-DSA-44 sign the canonical MailPayload commitment. HPKE remains in base mode;
sender authentication is not delegated to an HPKE authenticated mode.

## Multi-device

Mail content is encrypted once with one CEK.

The CEK is independently wrapped to authorized recipient devices.

Revoked devices are excluded from future mail.

## Recipient privacy

A public chain can accidentally expose a social graph.

Therefore raw human-readable `.cybou` recipient names should not be placed in every MailTx.

DEV derives the filter tag as SHA-256 over the fixed domain label, NetworkID,
and recipient AccountID. This supports deterministic local filtering but does
not hide the recipient, especially because the AccountID is already present in
the outer payload. Mainnet needs a separate reviewed privacy profile.

## Associated data

The exact DEV content AAD and HPKE `info` byte strings are frozen in the
MailEnvelopeV1 definition above. They bind protocol-critical context including:

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

ContentCommitment = SHA-256(
  "CYBOU/MAIL_COMMIT/V2"
  || salt[32]
  || ProtectedTextV1
)
```

The salt remains inside E2E encrypted content unless the recipient later chooses to disclose it.

This supports external evidence export while reducing dictionary-guessing risk.

## Historical sender-key proof

An exported evidence bundle must prove that the sender signing key was authorized for the AccountID at the historical finalization height, not only at the present time.
