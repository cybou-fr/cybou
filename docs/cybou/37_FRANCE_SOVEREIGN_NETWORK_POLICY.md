# 37 — France-first sovereign P2P policy

## Network model

CYBOU is a peer-to-peer network of independently validating full nodes.
Every participant runs the same node software. Storage and Central Authority PoA
finalization are optional operational capabilities.

Bootstrap is a rendezvous service distributing signed official network state and
seeding initial peer discovery. It does not vote, form a quorum, or finalize. The
Central Authority PoA key chain ($K_0 \to K_1 \to \dots$) remains the sole finality authority.

## France network policy

Production and DEV public P2P connections are restricted to IP addresses
classified as French by the node's local Geo dataset. The same policy applies
to inbound and outbound connections for bootstrap endpoints, storage
providers, ordinary peers, and the Central Authority.
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

## Official endpoints and bootstrap trust

Official network profiles specify known bootstrap endpoints with IP:port and
TLS SPKI pins. Transport authentication protects against connection tampering;
the Central Authority's signed NetworkBinding guarantees official network authenticity.
See [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md).

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
