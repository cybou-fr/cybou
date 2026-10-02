# 20 — Protocol serialization

## Canonical bytes

Every consensus object has one canonical byte representation before hashing or
signing. A profile specifies field order, integer encoding, byte order, bounds,
map ordering, duplicate-field rejection, and unknown-field handling. Parsers
check lengths and nesting limits before allocation.

The active formats are defined by their protocol authorities:

- block and PoA finality: `POA_FINALITY.md`;
- advisory validation attestations: `VALIDATION.md`;
- deterministic application encoding: `ROOT_PUBLICATION.md`;
- encrypted payload and chunk tree: `ENCRYPTED_CHUNK_TREE.md`;
- provider admission proofs: `STORAGE_ADMISSION.md`;
- Identity and names: `10_IDENTITY_NAMES.md` and `77_CYBOU_NAME_REGISTRY.md`.

The root-publication payload uses the bounded core-deterministic CBOR profile.
The encrypted-content schema remains opaque to consensus and is parsed only
after successful local decryption.

## Signing boundary

Production signatures cover canonical, domain-separated digests. Code never
signs a C++ object image, JSON document, or formatter-dependent representation.
Verification binds each signature to its network, object kind, and relevant
state context. Every production signature requires all components mandated by
the key role; missing post-quantum components fail closed.

## Version policy

Product releases, network definitions, consensus encodings, storage profiles,
and cryptographic suites have separate version lifecycles. Source and public
API names remain canonical and unversioned. Version bytes exist inside wire
and vault encodings only. State serialization uses canonical version 10 (`CYBOU_STATE_VERSION = 10`).
The protocol reset does not include legacy decoders, automatic import, or dual-format operation.
