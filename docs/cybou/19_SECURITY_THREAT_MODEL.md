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
- **Validator Identity (> 10M AUTH) compromised**: Attacker can sign false Validation for invalid operations. Every receiving node and PoA re-execute the operation, so a false signature creates no state, no relay and no storage admission; it only misleads a `Validated` indicator and is verifiable evidence for a future AUTH penalty. PoA can BURN AUTH of the compromised Identity.

## Transport and service identity

CYBOU P2P requires TLS 1.3 with the configured hybrid X25519+ML-KEM-768 group.
There is no PoA transport proof. Finalized block certificates alone prove
PoA authority. On-demand storage key proofs bind both HELLOs, the TLS exporter
and a fresh challenge; they identify replicas and never confer consensus power.
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
- Validation signatures never authorize storage admission.
- Repair degraded replicas through StorageService policy.

Development targets one remote full replica; Beta targets two independent
remote full replicas (plus local copy = 3 physical copies total).
A local cache does not count as a remote replica.

This is the durability target, not proof of physical independence in the
current implementation. Placement deduplicates proven StorageIds; separate
keys can belong to one host, disk or operator. A replica counts only with a
receipt signed by its proven StorageId; the receipt proves acceptance, not
later availability. Bounded background passes check selected chunks with
random-offset audits against the owner's local copy, and one in eight checks
(or any check without a local copy) uses full GET and BLAKE3. A successful
check is evidence for those bytes at that time, not continuous storage. Repair requires a valid
surviving copy and an available destination; a lost final copy cannot be repaired.

Finalized revocation removes admission authorization. Compliant providers
attempt to purge unshared admitted chunks, but this is neither proof of remote
physical deletion nor crypto-erasure. Historical capsules, KEM recovery seeds,
bridges and retained backups can preserve a decryption path. Deleting the current
local ContentKey cannot establish irreversible per-object erasure. Mail recipients
may retain delivered keys and plaintext. See the implementation assessment and
design proposal in [Data assurance and erasure](DATA_ASSURANCE_AND_ERASURE.md).

### Storage-economy threats (target, DEC-274–DEC-283)

- Self-dealing: a payer routing rent to its own provider. Mitigated by
  network assignment from finalized randomness and PoA-attested settlement.
- Onboarding laundering: bots converting the 20,000 start budget into
  transferable CYBOU. Mitigated by AccountCreate PoW and origin-restricted
  payouts (DEC-281).
- Fake service: compact filler or hash-only storage. Only unpredictable foreign
  ciphertext answered as exact bytes earns payment.
- Concentration: large operators capturing placements. Mitigated by random
  assignment without capacity weighting; the M6 simulation keeps the 5 largest
  of 1000 mixed nodes below 17% of replicas up to 70% demand.
- Sybil splitting: the same uniform selection pays an operator who splits one
  large node into many Identities (x73 share in the M6 simulation). AccountCreate
  PoW is the only current price; the selection rule is an open Beta gate.
- PoA as aggregator: a dishonest PoA can misreport service; Beta accepts this
  explicit trust, bounded by escrow limits and conservation checks.

## Simplified parser and signing boundaries

Bounded typed binary readers reject invalid lengths before allocation, unknown
versions, noncanonical presence flags, invalid UTF-8 and trailing bytes. Nodes
execute each transferred operation independently and verify every streamed
block against verified genesis and local finalized state. No transport role,
StorageId, endpoint or Validation signature substitutes execution or PoA proof.
The genesis specification digest anchors the durable signing journal. When a
canonical equivocation resolution differs from the signer's prepared history,
the journal fails closed rather than signing on a conflicting history.
