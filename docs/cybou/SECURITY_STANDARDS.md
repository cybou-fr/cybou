# Security standards and evidence

Reviewed: 2026-10-04. Authority: the standards-precedence rule in `AGENTS.md`.
Scope: cryptographic primitives, transport, key lifecycle and data deletion.
This is an initial applicability register, not an exhaustive standards audit.

Applicable current published standards govern technical security acceptance
above internal architecture, roadmap and product claims. Standards have defined
scopes: applicability must be justified, rather than importing every standard
into every subsystem. Use primary publisher sources and check their current
status, successor documents and errata at each relevant release review.
Separate adopted requirements, recommended guidance, experimental profiles and
verified implementation evidence. A newer draft is not a published standard.

| Reference and current status | CYBOU applicability | Evidence and next gate |
|---|---|---|
| [FIPS 203](https://csrc.nist.gov/pubs/fips/203/final), final ML-KEM standard, 2024-08-13; publisher lists potential updates | ML-KEM-768 component of hybrid KEMs | Check the exact backend, input validation, failure behavior and official vectors; this does not certify X-Wing or the TLS hybrid construction |
| [FIPS 204](https://csrc.nist.gov/pubs/fips/204/final), final ML-DSA standard, 2024-08-13; publisher lists potential updates | ML-DSA-44/65 roles | Review backend and vectors against final specification and publisher errata; the Ed25519 plus ML-DSA composition needs its own transcript and verification review |
| [RFC 9846](https://www.rfc-editor.org/info/rfc9846/), TLS 1.3, Proposed Standard, July 2026, obsoletes RFC 8446 | CYBOU P2P TLS 1.3 | Runtime restricts protocol to TLS 1.3 and group to X25519MLKEM768; review the library and CYBOU certificate/pin/exporter integration against the current text; protocol selection alone is insufficient |
| [RFC 9325 / BCP 195](https://www.rfc-editor.org/rfc/rfc9325.html), secure TLS deployment guidance | Transport configuration and authentication | Review applicable guidance together with RFC 9846; describe the application-specific identity model and reject any claim that TLS authenticates PoA authority |
| [NIST SP 800-88 Rev. 2](https://csrc.nist.gov/pubs/sp/800/88/r2/final), final September 2025, supersedes Rev. 1 | Media sanitization and evaluation of cryptographic-erasure claims | Managed unlink is not media sanitization. Recovery paths, key copies, wrapping keys and backups must be assessed before any erasure claim; current historical capsules and bridges preserve recovery |
| [X-Wing draft-connolly-cfrg-xwing-kem-11](https://datatracker.ietf.org/doc/draft-connolly-cfrg-xwing-kem/11/), 2026-09-23, Internet-Draft | Existing DEV Identity KEM profile | Experimental work in progress, not an IETF standard. Existing domain documents identify draft-05; establish the exact implemented construction and vectors before proposing any transition |

No row asserts FIPS module validation, product certification, complete RFC
conformance, or legal compliance. Published standards and recommendations
inform requirements; repository tests establish only the behavior they exercise.

## Conflict handling and release evidence

1. Record the reference, edition/status, retrieval date, applicable requirement,
   affected subsystem and source of implementation evidence.
2. Identify the concrete conflict or unverified requirement. Frozen internal
   decisions cannot justify accepting an unsafe or misleading security claim.
3. Correct the architecture and claim; specify compatibility, deployment and
   rollback implications before changing protocol or persistent bytes.
4. Validate with official vectors where applicable, malformed-input and failure
   tests, cross-implementation checks and supported-platform verification.
5. Keep the feature or claim gated while required evidence is missing. Record
   who reviewed it and what was checked before release.

Security corrections do not automatically authorize new network material,
rewriting immutable genesis, mechanically changing hash domains, introducing
legacy decoders, or silently replacing the DEV KEM profile.

## First follow-up review

Initial code inspection: `src/cybou/identity_kem.cpp` expands a 32-byte seed
using SHAKE256 into a 64-byte ML-KEM seed and 32-byte X25519 seed and calls
OpenSSL's ML-KEM-768 provider. Existing tests name vectors from
`draft-irtf-cfrg-concrete-hybrid-kems-03` and `-04`, while domain prose calls
the profile draft-05. These labels do not establish equivalence to the current
X-Wing draft; a byte-level specification/vector comparison remains open.
The targeted KEM tests exercise the existing profile, not current-draft
conformance. Do not mechanically change labels or crypto bytes to hide this gap.

- Resolve the draft-05 references in `09_CRYPTO_PQ.md`,
  `89_IDENTITY_KEM_PUBLICATION.md` and related documents against actual KEM code
  and vectors; distinguish obsolete implementation-status prose from real
  construction constraints.
- Review the actual TLS library version, hybrid-group specification and
  certificate/exporter authentication boundaries against RFC 9846.
- Complete the controlled key/capsule/bridge/backup inventory for erasure.
  The current [data-assurance assessment](DATA_ASSURANCE_AND_ERASURE.md)
  supplies recovery and managed-purge evidence, not a sanitization certificate.
