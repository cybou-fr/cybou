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

v1 does not freeze exact byte thresholds yet.

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
