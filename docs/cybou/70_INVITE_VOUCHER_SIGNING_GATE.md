# 70 — Invite Voucher signing gate

## Status

This document records the implementation gate discovered during the v0.0.3
audit. It does not freeze a signature algorithm or final consensus bytes.

## Threats closed by the gate

An Invite Voucher authorizes transfer of 6,000 CYBOU from `OnboardingPool` to
one account's `SystemBalance`. Signature verification alone is insufficient if
the signed payload is not bound to the intended beneficiary and network.

The final payload must prevent:

- interception and redemption by another AccountID;
- replay across CYBOU-DEV, test and production networks;
- replay after successful redemption;
- cross-protocol reuse as another Operator Authority action;
- alternate encodings of the same semantic voucher;
- caller bypass through an untrusted `signature_valid` boolean.

## Required payload fields

The canonical payload version must include at least:

```text
payload_version
network_id
voucher_id
beneficiary_account_id
grant_amount = 6000
expiry_epoch
organization presence flag
organization_id when present
```

The signature bundle is outside the payload it authenticates.

## Required processing order

```text
bounded wire decoding
-> canonical payload reconstruction
-> object-specific domain separation
-> signature-suite verification
-> typed verified-voucher result
-> beneficiary/network/epoch/grant checks
-> consumed-voucher lookup
-> atomic redemption and consumed-id recording
```

No Welcome Grant is credited before every stage succeeds.

## Canonical encoding gate

Before implementation reaches a consensus path, freeze and test:

- field order and fixed/variable widths;
- integer byte order;
- AccountID and network-ID encodings;
- optional-field encoding;
- exact signing-domain bytes;
- signature-suite ID and downgrade behavior;
- malformed, duplicate, oversized and unknown-field behavior;
- deterministic positive and negative byte vectors.

Do not sign C++ memory, JSON, Qt serialization, or any formatter-dependent
representation.

## Signature-suite gate

ANSSI recommends hybridization when post-quantum algorithms are deployed and
notes that a classical and post-quantum signature can be combined by requiring
both to verify. NIST FIPS 204 standardizes ML-DSA. OpenSSL 3.5 exposes ML-DSA
support and is an implementation candidate, not a protocol dependency.

`Ed25519 + ML-DSA-65` is retained as a candidate for Operator Authority
benchmarking and security review. It is not frozen until CYBOU evaluates exact
sizes, component-key binding, failure semantics, library behavior, test vectors
and the then-current standards status. Current IETF composite ML-DSA work must
be treated according to its publication status at freeze time; a changing draft
must not be copied into immutable consensus bytes.

## Current code rule

`InviteVoucherValidationContext::operator_authority_signature_valid` is a
temporary unit-test seam. It must be removed from every production-capable
validation path when the typed verifier boundary is introduced.

## Sources reviewed 2026-09-23

- ANSSI, *FAQ sur la cryptographie post-quantique*:
  https://cyber.gouv.fr/enjeux-technologiques/cryptographie-post-quantique/faq-pqc/
- NIST, *FIPS 204 — Module-Lattice-Based Digital Signature Standard*:
  https://csrc.nist.gov/pubs/fips/204/final
- OpenSSL, *OpenSSL 3.5 Final Release*:
  https://openssl-library.org/post/2025-04-08-openssl-35-final-release/
- IETF Datatracker, *Composite ML-DSA* current document status:
  https://datatracker.ietf.org/doc/draft-ietf-lamps-pq-composite-sigs/
