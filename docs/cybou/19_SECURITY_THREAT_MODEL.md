# CYBOU security threat model

## Trust boundaries

- The single PoA operator controls ordering and can censor or stop finality
  under the genesis-authorized key; it cannot forge Identity authorization or decrypt private content.
- Full nodes independently execute operations, verify blocks, and recompute
  state roots.
- Providers store encrypted chunks and apply local capacity and admission
  policy. Provider IDs prove keys, not independent hosts or operators.
- The desktop's encrypted Application DB is a local projection, not network
  storage truth.
- Bootstrap is an ordinary CYBOU full peer with a known locator. It distributes
  peer hints for initial discovery, but cannot alter or forge blocks or state transitions.

## Compromise impact analysis

- **Network Private Key compromised**: An attacker stealing the key after release can sign an alternative genesis specification, but existing compliant binaries contain the exact signed genesis and initial state as public constants and load no external replacement. Existing official networks cannot be updated in place. If compromised before release, an attacker could forge the initial network launch. The private key must remain strictly offline and under gitignored `/private/` at provisioning.
- **PoA key compromised**: Attacker can produce equivocating or censoring canonical block certificates within the current network. Equivocation triggers an immediate safety halt across compliant nodes. Because in-place PoA rotation is intentionally not supported, a compromised network cannot safely continue and requires launching a new NetworkID / new genesis cutover.
- **Bootstrap compromised**: Attacker can cause discovery denial-of-service, eclipse connecting peers, or partition initial discovery. Cannot forge network-signed genesis or PoA certificates. Outage does not affect an already formed P2P mesh.
- **Validator Identity (> 1M Authority) compromised**: Attacker can issue false advisory Validation attestations. This may cause peers with `validation.enabled = true` to accept provisional state temporarily, but CANNOT create canonical state. Once PoA publishes a conflicting block or drops the operation, the provisional state is discarded and rolled back unconditionally.

## Transport and service identity

CYP2 v3 requires TLS 1.3 with the configured hybrid X25519+ML-KEM-768 group.
Finalizer and provider role proofs are tied to both HELLOs and the TLS exporter.
The ephemeral TLS certificate alone is not a peer identity. Ordinary peers do
not have globally authenticated identities; discovered addresses are hints.
IP addresses, timing, and traffic sizes remain observable. A France-only
admission rule can reduce accepted public routes; it does not hide source IPs
or prevent routing through an allowed French endpoint.

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
- Default storage admission requires finalized RootPublication authorization Merkle proof.
- Optional provisional admission by participating providers is purged and rolled back upon PoA conflict.
- Repair degraded replicas through StorageService policy.

Development targets one remote full replica; Beta targets two independent
remote full replicas (plus local copy = 3 physical copies total).
A local cache does not count as a remote replica.
