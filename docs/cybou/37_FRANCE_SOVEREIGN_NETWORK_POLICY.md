# 37 — France-first sovereign P2P policy

## Network model

CYBOU is one peer-to-peer network of independently validating full nodes.
Every participant runs the same node software. Bootstrap, storage, advisory
Validation, and PoA finalization are optional local capabilities, not protocol
node classes.

Genesis authorizes one to four bootstrap Identities by stable AccountID and
expected RecoveryKeyID. These nodes can relay discovery, operations, and
finalized history, but they do not vote, form a quorum, or finalize. The
genesis-bound Central Authority PoA key remains the sole finality authority.

## France network policy

Production and DEV public P2P connections are restricted to IP addresses
classified as French by the node's local Geo dataset. The same policy applies
to inbound and outbound connections for bootstrap candidates, storage
providers, ordinary peers, Validation peers, and the Central Authority.
Hostname endpoints are resolved first and every numeric IPv4/IPv6 address is
checked. A `.fr` name does not establish location.

The region rule is node-local admission policy. It is not consensus, does not
prove a machine's physical location, and cannot stop traffic routed through an
allowed French endpoint. CYBOU makes no external GeoIP API calls. Production
and DEV fail closed for public P2P when mandatory Geo data is absent, corrupt,
or expired. LAB may explicitly bypass the rule for loopback and private test
networks.

Known VPN/proxy/Tor filtering is an optional local protection using local
classification data. It can reject only addresses present in that data and
does not guarantee detection of unknown tunnels or relays. Its setting and
data do not affect Identity, Authority, protocol admission, finality, or
canonical state.

## Genesis and endpoint trust

Initial IP:port and TLS SPKI pins locate candidate peers only before genesis.
The candidate proves the proposed AccountID and Recovery key over the pinned
TLS session. Genesis commits AccountID plus RecoveryKeyID; after AccountCreate
claims the grant, live bootstrap sessions prove the role using the current
Authorization key. Address and certificate changes do not change that grant.

The genesis bootstrap roster is fixed in v1. Adding or removing a grant
requires a signed network replacement. There is no BootstrapAdd or
BootstrapRemove operation. Replacement history must be verifiable by an
offline bootstrap node catching up across multiple generations.

## Operational resilience

One available bootstrap node is enough for rendezvous and relay. Multiple
bootstrap nodes improve availability but do not provide consensus resilience.
Clients rotate through configured and discovered endpoints, discard routes
for disconnected peers, and treat repeated relayed operations idempotently by
their existing OperationID semantics. If all bootstrap nodes are offline,
already connected direct P2P may continue; fresh discovery and relay are
unavailable. PoA finality still stops when the Central Authority signer is
offline.

France is the initial sovereign network boundary. Any future expansion beyond
French IP space requires an explicit architecture and policy decision, not a
GeoIP fallback or silent default.
