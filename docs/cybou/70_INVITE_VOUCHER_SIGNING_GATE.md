# 70 — Invite Voucher signing gate

## Status

This document records the implementation gate discovered during the v0.0.3
audit. The Operator Authority V1 envelope is frozen; cryptographic verification
and the redemption state transition are not yet production-ready.

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

The signature bundle is outside the canonical payload, but its suite and
authority keyset identity are themselves covered by the signed preimage:

```text
object_signing_domain
|| signature_suite_id_le16
|| authority_keyset_id
|| canonical_payload
```

For V1, `network_id` is the genesis block hash. A replacement genesis therefore
creates a new replay domain.

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

Operator Authority V1 freezes `Ed25519 + ML-DSA-65`, with both component
signatures required. Each bundle identifies an epoch-windowed authority keyset
containing both public keys, so historical vouchers remain verifiable after
rotation. This profile is not automatically inherited by MailTx, BFT votes,
release signing, or future authority-suite versions. Current IETF composite
ML-DSA work is not copied into these immutable V1 bytes.

## Current code rule

The caller-supplied `operator_authority_signature_valid` boolean has been
removed. Voucher validation now requires an `OperatorAuthoritySignatureVerifier`
and rejects missing, mismatched, or epoch-inactive keysets before invoking it.
The test implementation is deliberately local to unit tests; a production
Ed25519 + ML-DSA-65 provider remains required before redemption can ship.

## Sources reviewed 2026-09-23

- ANSSI, *FAQ sur la cryptographie post-quantique*:
  https://cyber.gouv.fr/enjeux-technologiques/cryptographie-post-quantique/faq-pqc/
- NIST, *FIPS 204 — Module-Lattice-Based Digital Signature Standard*:
  https://csrc.nist.gov/pubs/fips/204/final
- OpenSSL, *OpenSSL 3.5 Final Release*:
  https://openssl-library.org/post/2025-04-08-openssl-35-final-release/
- IETF Datatracker, *Composite ML-DSA* current document status:
  https://datatracker.ietf.org/doc/draft-ietf-lamps-pq-composite-sigs/
