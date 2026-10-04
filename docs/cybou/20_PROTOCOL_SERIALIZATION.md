# 20 — Protocol serialization

## Canonical bytes

Every consensus object has one canonical byte representation before hashing or
signing. A profile specifies field order, integer encoding, byte order, bounds,
unknown-tag handling and exact input consumption. Parsers check lengths
before allocation. Typed layouts have no recursive generic value parser.

The active formats are defined by their protocol authorities:

- block and PoA finality: `POA_FINALITY.md`;
- deterministic application encoding: `ROOT_PUBLICATION.md`;
- encrypted payload and chunk tree: `ENCRYPTED_CHUNK_TREE.md`;
- provider admission proofs: `STORAGE_ADMISSION.md`;
- Identity and names: `10_IDENTITY_NAMES.md` and `77_CYBOU_NAME_REGISTRY.md`.

The RootPublication payload uses fixed-order binary wire layout. Encrypted
ROOT/INDEX metadata and private Mail/Files/RecoveryBridge use typed binary
schema. Integers are little-endian; optional fields use a strict
0/1 presence byte, followed by their typed value when present. Strings are
length-prefixed UTF-8; invalid UTF-8, unknown types and trailing bytes fail.
The encrypted-content schema remains opaque to consensus and is parsed only
after successful local decryption.

## Signing boundary

Production signatures cover canonical, domain-separated digests. Code never
signs a C++ object image, JSON document, or formatter-dependent representation.
Verification binds each signature to its network, object kind, and relevant
state context. Every production signature requires all components mandated by
the key role; missing post-quantum components fail closed.

## Single-current-format policy

If only one supported form of a consensus object, state layout, wire message,
or application schema exists, it has no version identifier. It is simply the
current canonical format. Parsers expect and validate this single active layout.
A version field is introduced only when two formats must coexist simultaneously
in production or during a live migration window. Once migration concludes and
the old format is retired, the discriminator and legacy decoder are eliminated.

Code and protocol do not record development history. Type names, serialization
layouts, and wire headers do not carry historical version tags.

Cryptographic domain separation strings (`CYBOU/NETWORK-ID`, `CYBOU/OP-ID`,
`CYBOU/BLOCK`, etc.) are exact byte sequences of hash and key derivation
functions. They are not renamed incrementally during development; rather, they
use stable unversioned domain strings (`CYBOU/NETWORK-ID`, `CYBOU/OP-ID`,
`CYBOU/BLOCK`, etc.) after the coordinated DEVNET cutover.

## Native hash values

`Hash256` is an opaque 32-byte CYBOU type, without arithmetic semantics.
Protocol serialization, ordering and hex follow the stored byte order. Hex
output is lowercase and contains exactly 64 digits; parsing accepts exactly
64 hexadecimal digits, without a prefix, padding or byte reversal. AccountID,
BlockID, OperationID, state roots and NetworkBinding use this representation.
BLAKE3 ChunkID continues to hash exact stored encrypted bytes.

## Genesis chain anchor

Runtime and StateStore accept only `VerifiedNetworkGenesis`, whose signature
has been checked against the compiled Network Public Key. The verified genesis
specification digest is the height-zero chain tip and signing-journal anchor.
This is the existing domain-separated digest covered by the network signature;
there is no separately synthesized genesis block identifier or profile digest pin.
