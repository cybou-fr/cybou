# Security review implementation tracking

Review baseline: f15e33e41d4356d148f1182f998fa9c5b65aa51e. This working change
also implements the separately approved Identity Authority network version.
Running DEV retains its existing network and keys until explicit tested cutover.

| Finding | Current implementation state |
| --- | --- |
| NodeBinding emergency revocation | Owner-authorized revoke names the NodeID only and needs no service-node or provider proof-of-possession. Bind still requires both applicable proofs. |
| Remote resource enforcement | Canonical tickets are verified and durably consumed by the provider runtime, but CYP2 PUT/GET do not carry them yet. Do not connect bearer tickets to plaintext CYP2; secure authenticated/encrypted CYP2 and provider-side PUT/GET enforcement remain V7 cutover blockers. |
| Storage possession proof | Native BLAKE3 differential tests cover block boundaries, all leaves for small trees, random inputs and malformed proofs. This is useful regression coverage, not a substitute for an independent cryptographic audit or sustained fuzz campaign. |
| Validation journal rollback | Crash consistency and corruption detection are implemented. Restoring an older valid journal is not detectable; the threat model now says so. |
| Smoke secret fixture | CI creates its temporary PoA secret with mode 0600. The remote workflow still needs a successful run to confirm the multiprocess smoke and soak resume. |
| Domain separation | Network ID, state root and operation ID hash domains now use V7, V8 and V7 respectively, matching their serialization/network layers before the new network freezes. |
| Worst-valid-block performance | A bounded benchmark for maximum validation receipts, storage proofs and near-limit state maps remains required before V7 freeze. |
| Support Mail metadata | Documentation now acknowledges that public fee and padding-capsule patterns can statistically fingerprint support mail; the recipient is not explicitly encoded. |
| OS payment notifications | Incoming-payment notifications show a generic message so amount and counterparty stay out of lock-screen previews. Mail previews remain off by default. |
| Unsigned OP_RESULT discards journal | Remote rejection is delivery-uncertain. Claimed remote finalization cannot set Finalized without local verified inclusion. TCP regression tests cover both. |
| PoA entropy copies | Runtime config uses move-only cleansing Secret32; its buffer is cleared after finalizer construction. CLI finalizer entropy is held in the same RAII type. |
| OpenSSL 3.5 headers fail PQ tests | Use OpenSSL's documented seed/deterministic parameter names. Linux/CI verification still required; do not claim a green remote workflow without a run. |
| Secret files | Private creation, durable writes, bounded owned regular-file reads, symlink/reparse rejection, Windows owner ACL. Provider and finalizer/password readers use the primitive. Remaining auxiliary secret callers must be audited. |
| Generic root LAB worker | Helper separation remains to implement. Existing unrestricted sudo mode must never be granted as a narrow NOPASSWD capability. |
| LAB owner token | New tokens use 256 random bits, exclusive 0600 creation, regular-file/owner/mode checks on Unix, symlink refusal. Windows Python token ACL hardening remains. |
| Event correlation/privacy | Minimal mode excludes account, nonce, operation/chunk/provider identifiers and peer endpoint. LAB mode is explicit. Native event files use private owned append handles. |
| Ingress CPU DoS | Bounded per-IP connection and operation admission, before allocation/parsing/PQ verification. All attempts consume budget, including rejected operations. No global Authority penalty from local failure. |
| Desktop freshness | Unknown by default; account creation requires Current. Current comes from an explicit end-of-sync response, not batch size. It means current relative to the observed peer, not a wall-clock guarantee against eclipsing. |
| Operation status regression | Explicit transition cases reject Validated-to-Submitted and other backwards phases; canonical Finalized may override Failed. |
| Expected NetworkID | Signed release bundled manifest pins default desktop network. Explicit LAB selection remains available. Existing network/Identity files are never automatically archived or replaced. |
| Provider HELLO tampering | Hybrid proof v2 covers both complete HELLO byte strings with signer/verifier ordering. |
| Secure CYP2 session | Frame AEAD and genesis-authenticated finalizer transport still require implementation with a vetted secure transport. No custom CYBOU cryptography. |
| Replica independence | Distinct keys prove cryptographic identities; deployment must ensure independent hosts/disks/operators/failure domains. |
| Supply chain | Exact Actions commits, exact BLAKE3 commit+archive digest, and official OpenSSL tar checksum before build. Signed/reproducible release provenance remains to implement. |
| Report signatures | SHA companion is integrity only. Separate test/release evidence signing remains to implement; never reuse PoA key. |
| Authority checkpoint | Preview checkpoint path was removed. Authority derives from canonical state with NetworkID-bound policy in the new network. |
| Security documents | Vault AES-256-GCM, application ChaCha20-Poly1305, plaintext transport metadata boundary, current DEV PoA reality and new-version transition are distinguished. |

Passing tests are evidence for the tested paths only. The complete Authority,
provider resource admission, Linux LAB and security acceptance matrix is still
being built; this document must not be used as a deployment approval.
