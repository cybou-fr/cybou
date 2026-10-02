# CYBOU Validation

Validation is optional, advisory evidence produced by any CYBOU full node. It
describes whether that node independently accepted an operation against a
specified finalized base. A recipient decides locally whether to act on the
claim or wait for PoA finality.

There is no ValidatorSet, validator registry, protocol validator role,
admission operation, canonical validation state, reward, penalty, or resource
reservation. Validation does not change canonical state, operation admission,
PoA finality, or remote chunk authorization. Full nodes and the PoA finalizer
independently verify every operation and state transition.

## Evidence and trust

A future ValidationAttestation may include the operation ID, finalized base,
signer's ordinary Identity public key, and signature. It is an untrusted
application-level object until the recipient verifies the signature and
recomputes the operation result. No transport role proof or node binding is
needed for this advisory claim.

The recipient may display the signer's locally derived Authority as context.
Under the current informational policy, Authority of at least 1,000,000 marks
the opinion as validator-qualified for that recipient. This is a display and
trust choice, not network admission or consensus eligibility. A recipient can
choose any number of opinions and can ignore them all.

Validation never means `FINALIZED`. Only a locally verified PoA block makes an
operation final. A Validation claim does not satisfy the finality and Merkle
proof required by the current chunk-admission protocol. A provider may in the
future make a separate local decision to offer a provisional resource based
on Validation or other evidence, entirely at its own risk; that choice creates
no canonical right or durability guarantee and is not implemented.

## Scope

The current implementation has no Validation wire message or durable
Validation journal. If advisory attestations are built later, keep them
non-canonical, independently verifiable, and separate from PoA correctness.
