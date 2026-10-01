# CYBOU security threat model

## Trust boundaries

- The single genesis-bound PoA operator controls ordering and can censor or
  stop finality; it cannot forge Identity authorization or decrypt private
  content.
- Full nodes independently execute operations, verify blocks, and recompute
  state roots.
- Providers store encrypted chunks and apply local capacity and admission
  policy. Provider IDs prove keys, not independent hosts or operators.
- The desktop's encrypted Application DB is a local projection, not network
  storage truth.

## Transport and service identity

CYP2 v3 requires TLS 1.3 with the configured hybrid X25519+ML-KEM-768 group.
Finalizer and provider role proofs are tied to both HELLOs and the TLS exporter.
The ephemeral TLS certificate alone is not a peer identity. Ordinary peers do
not have globally authenticated identities; discovered addresses are hints.
IP addresses, timing, and traffic sizes remain observable.

## Content and local data

ChunkStore stores ciphertext only. Provider and network metadata must not
intentionally reveal filenames, folder paths, Mail content, recipient graphs,
or content keys. The GUI never enumerates arbitrary provider chunks.

IdentityOperationCoordinator serializes nonces, durably retains exact signed
bytes, and reconciles uncertain outcomes. Unsigned remote acknowledgments are
hints: a remote rejection or claimed finalization does not erase the journal.
Finalized status requires locally verified inclusion.

## Publication and storage

- Verify full BLAKE3 ChunkID before using fetched bytes.
- Verify content capsules and encrypted ROOT/INDEX/DATA structures.
- Keep finality, availability, and durability as separate states.
- Admit remote chunks only after finalized RootPublication authorization.
- Repair degraded replicas through StorageService policy.

Development targets one remote full replica; Beta targets two independent
remote full replicas. A local cache does not count as a remote replica.

## Authority and Validation

Authority is an informational derived metric. It cannot grant PoA power,
resource allocation, rewards, or penalties. Optional Validation is a signed
claim that a recipient verifies and assesses locally. A signed claim is not
proof of correctness; the PoA finalizer independently validates every
finalized operation. Neither mechanism changes canonical state or storage
admission.

## Recovery

Before rotating to a new Identity KEM epoch, protect historical decryption
capability required for clean recovery through the private RecoveryBridge
flow.
