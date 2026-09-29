# PoA finality interoperability vectors

These vectors freeze key derivation, hash input bytes, and certificate wire
encoding. All 32-byte IDs below are shown in canonical serialized byte order
(the order returned by `uint256::begin()`), not the display order of `GetHex()`.

## Key derivation

Input recovery entropy (32 bytes):

```text
000102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f
```

Purpose: `POA_FINALIZER` (`7`), using the existing Identity v2 HKDF-SHA256
derivation.

```text
Ed25519 public key:
8254c6e332edef49152acb98e85b9d566e094aeaa5aeac2bb3670c9929a633d6

SHA-256(ML-DSA-65 public key):
d4992c7d44ab1ceb9ed7bf307af728d8118374417e2290d61a98c5f3ab8de977

PoA finalizer key ID:
4b5cd4996b3b8c560c8a6e2071070795d5cf7f3d26b503c9002e2f214ea5c60c
```

The key ID is `SHA256("CYBOU/POA-FINALIZER-KEY-ID/V1" ||
Ed25519_public_key || ML-DSA-65_public_key)`.

## Finality digest

```text
NetworkID bytes:
0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20

Parent block ID bytes:
2122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f40
```

The canonical block is version 2, height 7, has the parent above, an empty
operation list, and this state root:

```text
4142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f60
```

Expected block ID bytes:

```text
64e819bc0ef92a3bc827de365094f94a45f7f085454a8a56f4b581ea9366cde3
```

The finality digest is SHA-256 of the ASCII domain followed by NetworkID,
block ID, `height = 7` as 8-byte little-endian, and parent block ID:

```text
8e91e9859bf416facf9331b6ed6906a10007244e286e4568b3bc7d744337d6a2
```

Ed25519 signature for the digest:

```text
1a78228567fe880a16481319d354f8a0b1291ee157022b22ceee278495fd15cb183db9dc21d86e2e3fed1f05264454e65fbe333f617c58536fc16f425f8a7409
```

ML-DSA-65 uses randomized signing in the current OpenSSL integration. Its
signature bytes therefore vary across signing calls for this same key and
digest. Tests verify generated signatures; they do not freeze a randomly
generated signature as a reproducible output. A cross-implementation fixed
ML-DSA signature vector remains an open cutover gate. OpenSSL documents the
randomized default and a deterministic test-only signing parameter in its
[ML-DSA EVP reference](https://docs.openssl.org/3.5/man7/EVP_SIGNATURE-ML-DSA/).

## Certificate encoding

The serializer test uses the same NetworkID, block ID, height, and parent as
above, with 64 bytes of `a5` as the Ed25519 signature field and 3,309 bytes of
`5a` as the ML-DSA-65 signature field. This is an encoding fixture only; these
synthetic signature bytes are not a valid certificate.

```text
version: 01
total encoded length: 3478 bytes
SHA-256(encoded certificate):
d189e4967722c7fcd16de274fadbc31d66373b911389cd00b9e19e0d7b81e115
```
