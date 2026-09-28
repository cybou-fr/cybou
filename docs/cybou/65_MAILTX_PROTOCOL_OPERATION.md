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

Exact byte layout is not frozen here.

The DEV cryptographic suite and one-account/all-active-device semantics are
frozen in `49_EMAIL_E2EE_HPKE_PQ.md` and `spec/email_crypto_profile.yaml`.
This does not freeze the Mail envelope's byte layout, HPKE context encoding,
discovery-tag derivation, or historical evidence format. Mail send and receive
remain disabled until those application-wire gates are complete.

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

DEV currently fixes `max_mail_ciphertext_size` at 64 KiB and charges the
deterministic integer fee `4 + ceil(ciphertext_bytes / 1024)`. Confirm that
the finalized envelope remains within this bound before enabling SendMail.

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
