# CYBOU Validation (Deferred)

Status: **Deferred / non-Beta research concept**.
This document describes an optional advisory validation mechanism that is NOT part
of the active protocol or Beta product.

Validation is optional, advisory evidence that could be produced by a full node. It
describes whether that node independently accepted an operation against a
specified finalized base. A recipient decides locally whether to act on the
claim or wait for PoA finality.

There is no ValidatorSet, validator registry, protocol validator role,
admission operation, canonical validation state, reward, penalty, or resource
reservation. Validation does not change canonical state, operation admission,
PoA finality, or remote chunk authorization. Full nodes and the PoA finalizer
independently verify every operation and state transition.

## Scope

The active implementation has no Validation wire message or durable
Validation journal. Advisory attestations remain deferred research.
