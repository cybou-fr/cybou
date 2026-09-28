> Historical/current-DEV scope: this document describes the BFT, MailTx, validator-set, or indexed-object protocol currently running on DEV. It is superseded for the next-gen target by `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md`. Product and UX requirements remain applicable only where they do not conflict with those target documents. No cutover is active yet.
# 16 — Native Mail Protocol

CYBOU Email is a chain-native protocol, not an overlay messenger.

## First-class operation

```text
MailTx
```

is a typed CYBOU protocol operation.

Do not encode it as OP_RETURN or arbitrary Bitcoin Script application data.

## v1 pipeline

```text
Protected text mail
-> random salt + content commitment
-> CEK + AEAD encryption
-> HPKE/PQ recipient key encapsulation
-> sender authentication
-> canonical MailTx serialization
-> fee/size validation
-> P2P propagation
-> BFT finality
```

The DEV cryptographic choices and MailEnvelopeV1 byte layout are frozen in
`49_EMAIL_E2EE_HPKE_PQ.md` and `spec/email_crypto_profile.yaml`: draft-05
X-Wing HPKE for one CEK capsule per recipient AccountID and key_epoch, plus one
ChaCha20-Poly1305 content encryption. Mail submission remains disabled until
an approved backend supports this suite and the envelope is integrated.
Incoming decryption additionally requires historical sender-key evidence
verification. DEV exposes the recipient AccountID and makes no relationship
privacy claim.

## Block/history vs state

```text
block/history:
    full encrypted MailTx

current state:
    only counters/authority/balance/PoT data
    required to validate future operations

local client:
    mailbox index
```

There is no permanent per-mail consensus-state record.

## Discovery

Recipient discovery uses a privacy-reviewed tag and a compact block/range filter or equivalent mechanism.

Exact design is still open.

## v1 limitations

```text
one recipient
text only
bounded size
no attachment
no SMTP
no realtime chat
```

This is the initial DEV/Alpha profile. It remains attachment-free by design;
Beta Mail requires a separately integrated Object Storage path for encrypted
attachments and is not complete at this profile. See `81_BETA_PRODUCT_SCOPE.md`.

## Beta Store integration

Beta MailTx commits to encrypted Store objects using opaque references; bulk
attachment bytes never enter blocks or consensus state.
