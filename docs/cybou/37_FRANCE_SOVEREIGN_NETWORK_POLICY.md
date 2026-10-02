# 37 — France-first sovereign P2P policy

## Network model

CYBOU is a peer-to-peer network of independently validating full nodes.
Every participant runs the same node software. Storage and Central Authority PoA
finalization are optional operational capabilities.

Bootstrap is an ordinary CYBOU full peer whose IP:port and TLS SPKI pin are known
in advance for initial rendezvous and peer discovery. It has no special consensus
role, no `CAP_BOOTSTRAP` flag, does not vote, and does not finalize. Genesis may assign
an ordinary initial Authority baseline to its Identity (e.g. 1,000,001 on DEVNET).
The genesis-authorized Central Authority PoA key remains the sole canonical finality signer.

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

Official network profiles (DEVNET, MAINNET) specify known bootstrap endpoints with
IP:port and TLS SPKI pins, and the compiled Network Public Key (`NetworkID = Network Public Key`).
Transport authentication protects against connection tampering; the offline Network
Private Key signature on the immutable genesis specification (with pinned `GenesisDigest`)
guarantees official network authenticity.
See [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md).

## Operational resilience

One available bootstrap peer is enough for initial rendezvous. Multiple
bootstrap endpoints improve availability but do not provide consensus resilience.
Clients connect, discover peers, establish direct P2P mesh connections, and
discard routes for disconnected peers. If bootstrap is offline, already connected
direct P2P continues uninterrupted. PoA finality stops only when the Central
Authority signer is offline.

France is the initial sovereign network boundary. Any future expansion beyond
French IP space requires an explicit architecture and policy decision, not a
GeoIP fallback or silent default.
