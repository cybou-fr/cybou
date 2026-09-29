# 57 — Proof of Trust policy

Proof of Trust is a deterministic anti-abuse and service-allocation signal based
only on protocol history. It is not social scoring, identity verification, or
consensus authority.

## Inputs and time

PoT uses account age and valid protocol history. Epochs derive from finalized
block height, and all arithmetic is deterministic integer arithmetic. Local
wall clocks and time zones never affect consensus. System Balance pays for
services but does not boost Beta PoT score.

## Service policy

Services may use protocol-defined account history to limit abuse. Generic
RootPublication consensus does not inspect encrypted Mail or Files schemas and
therefore cannot enforce Mail-specific quotas. Service limits belong to an
explicitly reviewed, privacy-compatible service policy; there is no implicit
MailTx/day counter.

Proof of Trust never grants block-finalization authority. The genesis-bound PoA
key alone authorizes finality in the current protocol.
