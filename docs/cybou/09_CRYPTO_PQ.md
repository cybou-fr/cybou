# 09 — Cryptography and PQ migration

## Principle

CYBOU is crypto-agile and targets PQ/T hybrid security for Email key establishment.

Do not invent primitives or combiners.

## Email target

```text
HPKE architecture
KEM target: X25519 + ML-KEM-768 hybrid
AEAD: ChaCha20-Poly1305 or AES-256-GCM
sender authenticity: crypto-agile signature layer
```

The exact production suite is frozen only after standards/implementation review.

## Standards status

- HPKE architecture: RFC 9180.
- ML-KEM: NIST FIPS 203.
- PQ/T KEMs for HPKE: active IETF work as of September 2026.

Because PQ HPKE identifiers/constructions are still in standards progression, protocol versioning must allow migration without reinterpretation of old ciphertexts.

## Downgrade protection

Suite choice is authenticated/bound.

Never silently fall back from PQ/T hybrid to classical-only for a recipient that requires the hybrid suite.

## Consensus separate

Email HPKE/PQ work is not a reason to force PQ signatures into every consensus vote before benchmarking.
