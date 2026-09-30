# CYP2 transport version 3

Status: implemented transport foundation for the separate Identity Authority
network version 7. This does not authorize DEV cutover or deployment.

## Session setup

- TLS 1.3 is established before CYP2 HELLO or other application frames.
- OpenSSL is configured to require `X25519MLKEM768`; unsupported builds fail
  closed instead of selecting a classical-only group.
- The TLS certificate is a per-process ephemeral self-signed certificate used only by
  TLS. It is not a CYBOU identity or trust anchor.
- There is no plaintext fallback. A peer that sends the old plaintext CYP2
  HELLO is rejected during TLS setup.
- The TLS exporter is included in the hybrid provider proof together with both
  complete HELLO messages and signer/verifier ordering. A provider proof from
  another TLS session therefore does not authenticate this one.
- A peer advertising operation acceptance must sign both HELLO messages and
  the TLS exporter with the genesis-bound PoA finalizer key. Peers verify that
  proof against their immutable NetworkDefinition before treating the endpoint
  as an operation-accepting finalizer.

TLS provides record encryption, integrity and in-session sequence protection.
Existing operation signatures, PoA certificate validation, validation
attestation checks, publication proofs and ChunkID verification remain
mandatory. Endpoint IP addresses, timing and record sizes remain visible.

## Remaining acceptance gates

- Authenticate a validation-node session against its finalized bound key and
  revocation status.
- Pass finalized ResourceTickets in storage PUT/GET and durably consume them at
  the authenticated provider before transfer.
- Add client reservation/ticket creation and recovery behavior.
- Test hybrid negotiation and identity binding across supported release
  builds and platforms; run the full acceptance matrix before any DEV cutover.

Until these gates pass, the transport foundation must not be described as
complete V7 network security or as a DEV deployment approval.
