# Identity Authority network version 7

Implementation target for a separate network. This does not authorize resetting
DEV, replacing its PoA key, or starting this binary against its version 6 state.
DEV cutover follows acceptance tests and explicit agreement.

## Terms and canonical accounting

Identity is the stable account-level AccountID. A bound NodeID is a service
signing role (Ed25519 + ML-DSA-44), never a device or delegated Identity key.
A bound storage role additionally proves possession of a STORAGE_PROVIDER key.
ProviderID identifies that cryptographic key, not a physical failure domain.
Only the genesis-bound hybrid PoA certificate finalizes canonical state.
Validation is signed pre-finalization evidence; local Validated is neither
inclusion nor remote chunk admission. Protected describes verified remote
ciphertext availability, separately from Finalized.

Authority is non-transferable and independent from Balance and System Balance.
Its canonical accumulator keeps earnings and penalty debt separately. Effective
Authority is max(earned - debt, 0); zero effective value does not erase debt.
Age adds one per completed finalized-height epoch, qualifying authorized activity
adds one per operation with a cap of 16 per epoch, and voluntary SystemLock adds
the whole-CYBOU amount once. Automatic onboarding System Balance adds no points.

## Evidence

Liveness counts timely signed responses to the latest finalized height, once
per AccountID/height across the union of its bound nodes. A completed epoch with
at least ceil(epoch_blocks/2) distinct observations earns at most one point.
It measures finalized-height participation, not independently certified wall
clock uptime. The last epoch height may respond in the first child block.

Storage declarations require a finalized RootPublication inclusion path and a
BLAKE3 proof of the final ciphertext leaf, binding exact stored byte length.
Sixteen epoch challenge slots use the parent finalized block ID to select a
BLAKE3 leaf. All slot responses must be timely and valid. Completed byte×epoch
work accrues one point per GiB, retaining fractional work. Missing responses give
zero work and no fraud penalty. A provably false signed storage claim adds 32
points of persistent penalty debt and earns no work for that epoch. Proofs show
possession of selected encrypted bytes; they do not prove physical independence,
exclusive storage, or resistance to outsourcing. Provider selection remains local.

A validation receipt binds exact subject bytes and one finalized base. Nodes
independently execute the subject. A valid receipt earns one contribution point
only when its exact subject is included in the same block, belongs to another
Identity, and the 16-point per-epoch cap is not exhausted. A false positive signed
claim adds 32 debt. AccountCreate and nested service receipts are excluded.

## Generic resources

NetworkDefinition freezes the Authority policy. Tier is floor(log2(effective+1)),
capped at 16. Default allowances and immutable ceilings are:

| Domain | Base | Added per tier | Hard ceiling |
| --- | ---: | ---: | ---: |
| ProtocolBudget | 2048 operations/epoch | 128 | 65536 |
| StorageBudget | 256 MiB | 512 MiB | 64 GiB |
| BandwidthBudget | 2 GiB/epoch | 2 GiB | 256 GiB/epoch |

Eligibility is frozen to the block parent; same-block contributions cannot raise
their own allowance. Epochs contain at most 65536 heights. Arithmetic is bounded.

ResourceReservation is an Identity-authorized batch of up to 32 opaque grants.
Each reserves a generic domain, bytes and use commitment, costing four System
Balance units (three Security, one Onboarding). GrantID binds exact operation ID
and grant index. At most 8192 live grants per Identity and one million globally
are allowed. Storage grants persist until authorized ResourceRelease; bandwidth
grants expire at the next epoch and cancellation never refunds its epoch usage.
Commitments bind NetworkID, owner, ProviderID, transfer verb, publication, ChunkID,
exact byte count and a fresh nonce for bandwidth. Consensus has no Mail/File,
filename, recipient, or provider placement entity.

A provider must require finalized publication admission plus matching storage
and bandwidth grants, durably consume a one-use bandwidth ticket before transfer,
and reject replay. Client reservation, provider transport admission and recovery
integration are acceptance gates still being completed. Canonical reservation
accounting alone must not be presented as a fully enforced remote network limit.
