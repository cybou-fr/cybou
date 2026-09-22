# 68 — Operator key separation

Commercial ownership does not justify one master key.

CYBOU must separate operator security domains.

## Minimum key domains

```text
Operator Authority Key
    validator admission/removal
    Invite Voucher authority
    protocol-defined operator governance actions

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
