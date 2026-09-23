# 68 — Operator key separation

Commercial ownership does not justify one master key.

CYBOU must separate operator security domains.

## Minimum key domains

```text
Operator Authority Key
    validator admission/removal
    protocol-defined operator governance actions
    (MUST NOT participate in ordinary AccountID creation or onboarding)

Operator Validator Key
    BFT consensus votes

Release Signing Key
    official software/update authenticity

Treasury Key
    company-owned CYBOU Balance and operational treasury
```

These keys must not be interchangeable.

## Operator Authority protection

Even with a single owner/company, the Operator Authority should use multi-key protection.

Preferred operational baseline:

```text
2-of-3 operator authority
```

using independent hardware/offline key custody.

This is not a DAO.

It is protection against:

- one stolen laptop;
- one lost key;
- one compromised device;
- one accidental deletion.

## No master powers

Operator Authority cannot:

- decrypt E2E Email;
- forge user signatures;
- arbitrarily debit user Balance;
- bypass BFT validation.

Its powers are explicitly enumerated by protocol rules.

## Object-level signing domains

Key separation does not replace per-object domain separation. Operator
Authority operations must use distinct frozen signing domains, including at
least:

```text
Validator Admission
Validator Removal
Protocol Parameter Action
```

The exact domain bytes are part of each canonical object specification. A
signature valid for one authority operation must not be reusable as another.

## Signature-suite status

The primitive hybrid signature verifier (`OpenSslHybridSignatureVerifier` for `Ed25519 + ML-DSA-65`) is implemented in `src/cybou/signing.{h,cpp}` backed by OpenSSL >= 3.5. Consensus-operation wiring and typed boundary integration into block validation remain pending. Production consensus must receive a typed verified result from the Operator Authority verifier after canonical serialization and domain-separated verification.

For rare Operator Authority operations, V1 freezes `Ed25519 + ML-DSA-65` with
both signatures required. The signature bundle names an epoch-windowed keyset
containing both public keys. MailTx, BFT votes and release signing remain
separate performance/security profiles.
