# 37 — France-first sovereign P2P policy

## Network model

CYBOU is a peer-to-peer network of independently validating full nodes.
Every participant runs the same node software. Storage is intrinsic to every Full Node; a local PoA signer is optional.

Bootstrap is an ordinary CYBOU full peer whose IP:port and TLS SPKI pin are known
in advance for initial rendezvous and peer discovery. It has no special consensus
role, does not vote, and does not finalize. Any AUTH its
Identity holds is an ordinary GenesisAllocation decision, not a bootstrap property.
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
or expired. Development follows the same France-only admission policy.

Known VPN/proxy/Tor filtering is an optional local protection using local
classification data. It can reject only addresses present in that data and
does not guarantee detection of unknown tunnels or relays. Its setting and
data do not affect Identity, Authority, protocol admission, finality, or
canonical state.

## Official endpoints and bootstrap trust

The compiled DEVNET official network defines bootstrap `51.255.46.58:29461`
with its TLS SPKI pin, Network Public Key (`NetworkID`), signed immutable
genesis and initial state. MAINNET is unprovisioned and has no bootstrap
locator. Transport authentication protects discovery; the offline Network
Private Key signature on the compiled genesis guarantees its authenticity.
See [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md).

## Operational resilience

The automatic local Geo updater verifies every candidate before activation.
A failed update never replaces an already validated dataset. Transient download,
publication or checksum inconsistency is retried by fetching both release metadata
and the archive again, up to five attempts (0, 2, 5, 15 and 60 second delays).
After exhausted retries, the next cycle starts in one hour with a valid current
dataset, or five minutes without one. Success or a current release returns to the
14-day check interval. Expired datasets are never usable: without a valid dataset,
public P2P remains fail-closed. Candidate CSVs retain SHA-256 cache naming and are
validated before atomic installation; startup removes only matching Geo `.part` files.

One available bootstrap peer is enough for initial rendezvous. Multiple
bootstrap endpoints improve availability but do not provide consensus resilience.
Clients connect, discover peers, establish direct P2P mesh connections, and
discard routes for disconnected peers. If bootstrap is offline, already connected
direct P2P continues uninterrupted. PoA finality stops only when the Central
Authority signer is offline.

France is the initial sovereign network boundary. Any future expansion beyond
French IP space requires an explicit architecture and policy decision, not a
GeoIP fallback or silent default.
