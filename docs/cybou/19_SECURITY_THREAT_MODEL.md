# CYBOU security threat model

## Trust boundaries

- PoA operator controls canonical ordering/finality and can censor or stop
  finality, but cannot forge Identity authorization or decrypt private content.
- Full nodes independently execute and verify canonical state.
- Providers store opaque encrypted chunks and required admission/accounting
  metadata.
- The desktop private Application DB is an Identity-local encrypted projection,
  not network storage truth.

## Content confidentiality

ChunkStore contains encrypted stored bytes only.

Provider/network metadata must not intentionally reveal:

```text
plaintext filenames
folder paths
Mail subjects/bodies
recipient contact graph
content keys
```

The GUI never enumerates arbitrary provider chunks.

## Key substitution

Recipient KEM capability must come from verified finalized Identity history and
capsules remain bound to network, publication root, sender authorization context
and recipient key epoch.

## Local disk disclosure

- common ChunkStore: ciphertext only;
- Identity Application DB: encrypted at rest;
- plaintext only in bounded memory or explicit user output;
- locking Identity removes normal access to private application data.

## Publication/replay

IdentityOperationCoordinator owns nonce serialization, durable exact-operation
retry and uncertain-outcome reconciliation.

## Storage corruption/loss

- verify BLAKE3 before using a chunk;
- authenticate/decrypt ROOT/INDEX/DATA tree;
- distinguish finality from durability;
- maintain remote replica target;
- audit/health-check and repair degraded protection.

Development target: 1 remote replica.
Beta target: 2 independent remote replicas.
Local cache is not a remote replica.

## Authority farming

### Self-spam

Activity credit is capped per epoch. Additional valid operations may still run
subject to resource budgets but stop increasing activity Authority.

### Chunk fragmentation

Storage Authority is based on verified byte×epoch contribution, never raw chunk
count.

### Node-count farming

Liveness credit is at most +1 per Identity per epoch, based on union uptime of
bound nodes, not number of nodes.

### Wealth dominance

Raw Authority may include one-time System Balance contribution, but network
privileges use a saturating/logarithmic tier with immutable ceilings.

## False storage claims

Temporary timeout is not automatically fraud.

A provably false signed storage claim receives no positive credit and incurs
the immutable penalty defined by Authority policy. Global penalty requires
canonical attributable evidence.

## Invalid future validation

Future provisional validation, if implemented, must be signed and attributable
and is only a claim about a named operation and finalized base. See
[`PROVISIONAL_VALIDATION.md`](PROVISIONAL_VALIDATION.md). A signature does not
make a claim true; PoA independently validates all finalized operations.
Conflicting or stale operations and transport failures are not proof of fraud.
An attestation alone does not create an Authority reward or penalty; any later
global effect requires immutable policy and canonical attributable evidence.
Provisional validation never becomes PoA power.

## Recovery after key rotation

Before rotating to a new Identity KEM epoch, historical decryption capability
required for clean recovery must be protected through the private
RecoveryBridge flow.
