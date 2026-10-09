# PoA finality interoperability vectors

Status: EVIDENCE
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

These regression vectors freeze the current canonical domains and layouts.
`src/test/cybou_poa_tests.cpp` verifies hybrid keys, both signatures, digests
and exact certificate encoding. The hash/layout values below were also computed
independently with Python hashlib and struct, without calling CYBOU code.

Input entropy is bytes 00 through 1f. The PoA key purpose is 7, using
HKDF-SHA256 salt `CYBOU/IDENTITY/HKDF-SHA256` and the role-specific
`CYBOU/IDENTITY/POA_FINALIZER/` component label.

```text
Ed25519 public key:
89ea88038e42fb8c57b0a84c49f025c4c5f98c94a031ecc49fb1ebe402bba03e
SHA-256(ML-DSA-65 public key):
ac5b4f17b7501f3a5be7d77f86ce25b460272e1ecf4c83fe58bbb0655a7fab61
PoA key ID:
65ffb888a16c267ac1dcdd72f3bb5fc690af64a248c5e81541666274924518f8
```

The key ID hashes `CYBOU/POA-FINALIZER-KEY-ID` followed by both public keys.
For the empty block at height 7:

```text
NetworkBinding: 0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20
Parent BlockID: 2122232425262728292a2b2c2d2e2f303132333435363738393a3b3c3d3e3f40
State root: 4142434445464748494a4b4c4d4e4f505152535455565758595a5b5c5d5e5f60
BlockID: 594227378370f7b88b584008d721c1520237a87fd668ae596d491259d0a5fd6c
PoA digest: e8f9ff35c45f7804770f9b88e5960c6f03d570a9b5588c772eda364896739b15
Ed25519 signature:
0822445f57d27b619fca77a7cb2264177be17077f22e3c102084216613b22d3444ada0c23b12810c352f4abf9c23695417a5c94fcd3eb3b4ebd499ef8a447305
SHA-256(deterministic test-only ML-DSA-65 signature):
4beeb115b302e2d09c61836a997835698d6d795fd4cc2539160c501cab187d8a
```

Production ML-DSA signing retains OpenSSL's randomized default. The regression
fixture uses deterministic test signing only. It is not a production mode.

The encoding fixture uses 64 bytes of a5 and 3309 bytes of 5a as synthetic
signature fields; these bytes are not a valid certificate. There is no format
discriminator. Field order is NetworkBinding, BlockID, height (u64 LE), parent,
Ed25519 signature, ML-DSA-65 signature.

```text
Encoded certificate length: 3477 bytes
SHA-256(encoded certificate): eab7555af2b5831947b86984236f33a5321684bb0b88d3a0c088666a59216bf8
```
