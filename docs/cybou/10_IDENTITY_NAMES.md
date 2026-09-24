# 10 — Identity and `.cybou` names

Implementation status: typed AccountID, AccountCreate, local identity keys,
and proof of possession exist in DEV code. `.cybou` alias registration,
device rotation, and a global name registry are design targets, not live
features of the current native runtime.

## Cryptographic identity

```text
AccountID = cryptographic identity
```

V1 encodes AccountID as an opaque, stable 32-byte value in internal byte order.
The all-zero value is invalid. AccountID names an authorization record rather
than one public key, so authorized device/payment/mail keys may rotate without
changing the AccountID.

An AccountID may authorize:

- device keys;
- mail signing/encryption keys;
- payment keys/authority;
- optional human-readable `.cybou` alias.

Identity is not equal to one permanent private key.

## Human-readable alias

Example:

```text
stan.cybou
```

Suggested syntax direction:

```text
[a-z0-9-]
3..32 characters before .cybou
```

Exact registration/renewal policy remains versioned protocol design.

## Historical key state

The chain/state must preserve enough authenticated history/commitments to prove which signing keys were authorized for an AccountID at a historical MailTx height.

This is required for MailEvidenceBundle verification.

## Device lifecycle

Support:

```text
add device
revoke device
rotate encryption/signing keys
```

Revocation affects future authorization.

It cannot erase data or signatures already obtained in the past.

## Mail relation

Native CYBOU Email is a first-class consensus-registered protocol operation.

Identity authorization therefore directly participates in validating MailTx sender authentication.

## Directory

A global searchable real-name directory is not required.

`.cybou` names are aliases, not proof of civil identity.
