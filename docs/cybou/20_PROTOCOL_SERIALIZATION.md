# 20 — Protocol serialization and versioning

This document must be implemented before CYBOU freezes public consensus formats.

## Canonical encoding rules

Define explicitly:

- byte order;
- integer encodings;
- maximum lengths;
- maximum nesting depth;
- string encoding;
- canonical ordering where maps/sets exist;
- unknown-field behavior;
- duplicate-field behavior;
- signature/public-key algorithm identifiers;
- network/protocol version negotiation;
- feature activation;
- hard-fork vs soft-compatible change rules.

## Consensus safety

A consensus object must have one canonical byte representation for hashing/signing.

Do not permit:

```text
same semantic object
-> multiple valid encodings
-> different hashes/signatures
```

unless explicitly designed and safe.

## Bounds

Every peer-controlled length field has a protocol maximum checked before allocation.

PQ-ready variable-length keys/signatures require especially strict bounds.

## Domain separation

Signatures/hashes for different purposes must use explicit domain separation, for example:

```text
CYBOU/TX/...
CYBOU/BLOCK/...
CYBOU/VOTE/...
CYBOU/NODE-HANDSHAKE/...
CYBOU/STORAGE-PROOF/...
CYBOU/MESSAGE/...
```

Exact strings/bytes are frozen with the protocol spec.

Key-domain separation and object-signature separation are distinct requirements.
An Operator Authority key domain does not by itself separate an Invite Voucher
signature from validator admission, validator removal, or another authority
operation. Every signed object type therefore requires its own frozen signing
domain.

## Signed-object boundary

Consensus code must never sign a C++ object memory image, JSON document, or
formatter-dependent representation. A signed object is split into:

```text
versioned payload without signature
-> canonical bytes
-> object-specific signing domain
-> signature-suite verification
-> typed cryptographically verified result
-> structural/state transition validation
```

A caller-provided boolean such as `signature_valid=true` is permitted only as a
unit-test seam. It must not be accepted by a production-capable consensus path.

## Version policy

Do not equate application version with protocol version.

Track separately:

- product version;
- network protocol version;
- consensus version;
- storage protocol version;
- messaging protocol version;
- crypto suite version.
