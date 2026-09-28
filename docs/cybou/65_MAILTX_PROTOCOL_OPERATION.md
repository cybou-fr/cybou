# 65 — MailTx as a first-class CYBOU protocol operation

CYBOU must not encode native Email as arbitrary Bitcoin Script data, OP_RETURN payloads or application blobs hidden inside ordinary payment transactions.

## Frozen rule

```text
MailTx is a first-class CYBOU protocol operation.
```

The Bitcoin Core codebase is an engineering base, not a requirement to preserve Bitcoin transaction semantics.

## Protocol operation families

Target architecture:

```text
CybouProtocolOperation
├── Payment
├── Identity
├── SystemBalance
├── Mail
├── ValidatorSet
├── OperatorAuthority
└── ProtocolParameter
```

The exact C++ representation may be a tagged/typed transaction or another clean serialization chosen during implementation.

## Mail operation v1

Conceptually:

```text
MailTx {
    protocol_version
    mail_id
    sender_account
    recipient_discovery_tag
    crypto_suite
    encrypted_subject_and_text_body
    recipient_key_capsules
    content_commitment
    ciphertext_hash
    sender_signature
    fee
}
```

The canonical outer MailTx serialization is implemented in `mail_tx.h`; the
DEV ciphertext field carries the frozen `MailEnvelopeV1` format specified in
`49_EMAIL_E2EE_HPKE_PQ.md`.

The DEV cryptographic suite and one-capsule-per-recipient-account semantics are
frozen in `49_EMAIL_E2EE_HPKE_PQ.md` and `spec/email_crypto_profile.yaml`.
The HPKE context encoding and discovery-tag derivation are now frozen for
DEV. The historical evidence encoding remains an off-wire receive gate. Mail
send remains disabled until the selected X-Wing HPKE backend is available and
integrated; incoming decryption also requires evidence verification.

## Requirements

MailTx must have:

- canonical serialization;
- explicit type/domain separation;
- deterministic validation;
- maximum serialized size;
- replay/duplicate protection;
- sender authorization validation;
- recipient-discovery privacy review;
- deterministic integer fee calculation;
- block-weight/resource accounting.

## Size-aware fee

DEV fixes `max_mail_ciphertext_size` at 64 KiB and charges the deterministic
integer fee `4 + ceil(ciphertext_bytes / 1024)`. `MailEnvelopeV1` caps its full
serialized envelope at this limit; the enclosing AuthorizedMail adds a fixed
2,698-byte payload/header prefix.

The fee structure is:

```text
MailFee =
    base_fee
    + integer_size_tier
```

There is a strict `MAX_MAILTX_SIZE`.

Do not allow a large text MailTx to cost the same as a tiny one without a resource model.

## No priority fee in v1

v1 uses a deterministic fee schedule.

```text
priority fee = disabled
fee bidding = disabled
```

A fee market may be introduced only if measured congestion justifies it.

## Future Store

After Store:

```text
MailTx {
    ...
    content_root
    encrypted_manifest_commitment
    ...
}
```

The large body/attachments are no longer embedded in block data.
