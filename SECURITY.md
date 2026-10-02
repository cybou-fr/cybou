# Security policy — CYBOU

CYBOU is experimental software, not a production communication or storage
service. The target uses single-operator hybrid-PQ PoA and independently
validating full nodes; it does not provide Byzantine fault tolerance.

## Reporting a vulnerability

Report security issues privately to `security@cybou.org` or the designated
project security contact. Include the affected component, impact and steps to
reproduce. Do not include real user secrets or recovery phrases.

## Trust and failure boundaries

- An official profile pins bootstrap IP:port, TLS SPKI and immutable Network
  Root public key `R`. The pin authenticates the endpoint; only `R` verifies
  official binding and Authority assignments.
- Bootstrap compromise may deny service or advertise false availability, but
  cannot produce a root-valid official network. Its outage does not halt an
  already connected P2P mesh.
- Compromise of current PoA key `P` threatens finality in its assigned epoch.
  `P` cannot replace the network, appoint its successor or sign a root-valid
  binding. Journal rollback and conflicting signing must fail closed.
- Compromise of private `R` is critical compromise of official network
  authority. Keep it separate from routine desktop finalization.
- Full nodes check root assignments, both PoA signature components,
  operation execution and state roots independently.
- Identity Recovery, Authorization, KEM, Network Root, PoA, Release Signing
  and Treasury have separate key purposes and material. No classical-only
  production signature fallback is permitted.

Public P2P admission is France-only for inbound and outbound DEV/production
connections, failing closed on unavailable or corrupt local Geo data. LAB
loopback/private bypass must be explicit. Optional VPN/proxy/Tor filtering is
local policy and changes no canonical state.

RootPublication exposes generic accounting and opaque chunk IDs. Application
schemas, recipients, filenames and graph edges remain encrypted. Providers
verify full ChunkIDs and finalized-publication authorization proofs. Finality
authorizes admission, not availability or durability.

CYP2 v3 requires TLS 1.3 and `X25519MLKEM768`. Finalizer and provider role
proofs bind to the TLS exporter and both HELLOs. Ordinary peers are not
globally authenticated by their ephemeral certificates. Secret files require
owner-only permissions and must reject links or reparse points.

## Current DEV limit

The DEV VPS runs an experimental standalone bootstrap prototype. Its existing
state must remain operational until acceptance and coordinated cutover. See
`AGENTS.md` and `docs/cybou/26_IMPLEMENTATION_STATUS.md`.
