# CYBOU security threat model

## Trust boundaries

- The single PoA operator controls ordering and can censor or stop finality
  under the active Authority key; it cannot forge Identity authorization or decrypt private content.
- Full nodes independently execute operations, verify blocks, and recompute
  state roots.
- Providers store encrypted chunks and apply local capacity and admission
  policy. Provider IDs prove keys, not independent hosts or operators.
- The desktop's encrypted Application DB is a local projection, not network
  storage truth.
- Bootstrap is discovery rendezvous infrastructure. It distributes signed
  official network state and cannot alter or forge blocks or state transitions.
- A compromised bootstrap can deny service or lie about availability, but
  cannot create a binding valid under immutable Network Root `R`. A bootstrap
  outage does not stop an existing direct mesh.
- Compromise of current PoA key `P` threatens finality within its assigned
  epoch but cannot authorize a new network or successor key. Compromise of
  private `R` is critical compromise of official network authority.
- Production/DEV public peer admission uses local French-IP classification.
  This is a node-local routing policy, not a consensus guarantee or proof of a
  peer's physical location. Missing or corrupt mandatory Geo data fails
  closed; LAB bypass is explicit and limited to test networking.
- Optional VPN/proxy/Tor filtering is only as complete as its local data. It
  does not detect unknown relays or tunnels and does not affect canonical
  state, Identity, Authority, or PoA.

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
- Admit remote chunks only after finalized RootPublication authorization.
- Repair degraded replicas through StorageService policy.

Development targets one remote full replica; Beta targets two independent
remote full replicas. A local cache does not count as a remote replica.

## Derived Authority

Authority is an informational derived metric. It cannot grant PoA power,
resource allocation, rewards, or penalties. The PoA finalizer independently
validates every finalized operation.

## Recovery

Before rotating to a new Identity KEM epoch, protect historical decryption
capability required for clean recovery through the private RecoveryBridge
flow.
