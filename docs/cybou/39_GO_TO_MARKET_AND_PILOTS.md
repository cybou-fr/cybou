# 39 — Go-to-market and pilot strategy

## Category

```text
Sovereign cybersecurity, communications and data infrastructure

Shared open-source trust core
        ↓
CYBOU Public (reference network) + CYBOU Enterprise (private networks)
```

CYBOU provides protocol-level sovereign infrastructure for private communications,
cryptographic identity, and sensitive file storage without foreign SaaS dependencies.

## B2B business model

The commercial strategy builds on an open-source trust core with commercial Enterprise solutions:

```text
Open-source CYBOU trust core
        ↓
Auditable technology and public reference network
        ↓
CYBOU Enterprise
        ↓
Private organization-controlled CYBOU networks
        ↓
Commercial deployment, integration, governance tooling, and SLA support
```

### Open vs. Enterprise boundary

- **Open-source common core:** protocol specification, wire formats, Full Node
  runtime, consensus and block verification, canonical state execution, cryptographic
  Identity verification, hybrid PQ primitives and compositions, RootPublication,
  storage protocol, and P2P transport.
- **Enterprise-specific modules (under development):** fleet management, centralized
  administration console, organization provisioning, SSO/LDAP/Entra integration,
  HSM integration, policy governance, monitoring, compliance reporting, deployment
  automation, and dedicated enterprise support.

Organizations verify the core technology through open source and subscribe to
commercial Enterprise products and services for deployment, operations, and management.

## First adoption unit

An organization/team with an existing contact graph:

```text
first_adoption_unit: organization/team
isolated_consumer_launch: false
```

Do not optimize the launch around isolated individual users. A team or organization
adopts CYBOU together so users have colleagues and partners to communicate with on day one.

Priority target segments:
- French and European organizations;
- PME and ETI handling confidential business secrets or intellectual property;
- Regulated or cybersecurity-sensitive industries (defense, aerospace, healthcare, energy);
- Critical infrastructure operators (OIV/OSE, NIS 2 preparation);
- Executive committees, legal counsel, and M&A data rooms.

*Use cases represent strategic target markets, not completed regulatory certifications.*

## Pilot framework (PoC / Design Partners)

Immediate commercial traction targets design partners and structured B2B pilots:

```text
Scale:                 20–100 users
Target:                French organization / enterprise team
Traffic:               Controlled operational volume
Environment:           DEVNET or dedicated private pilot network
Duration:              8–12 weeks
Governance:            Single-operator PoA disclosed transparently in pilot materials
Support:               Direct engineering support and structured feedback
```

## Pitch

Lead with:

```text
Sovereign cybersecurity and communications infrastructure designed in France
Protocol-level architecture, not a SaaS wrapper over an existing cloud platform
Client-side end-to-end encryption with zero central decryption keys
Network-native cryptographic identity (.cybou)
Independent full-node verification of blocks and state transitions
Hybrid classical and post-quantum protection (Ed25519 + ML-DSA, X-Wing)
Distributed encrypted storage with integrity verification (BLAKE3)
Private network flexibility: organization-chosen admission (VPN, LAN, CIDR)
Familiar Mail and Files workflows over native cryptographic primitives
Clean-machine recovery from Identity vault alone
```

Do not lead with:
- blockchain, coin, token, staking, or UTXO economics;
- consumer messenger comparisons;
- unverified certification claims.

## Expectation management

```text
CYBOU Email is CYBOU-native.
It is not an SMTP/IMAP replacement gateway.
DEVNET is an active development network; MAINNET is not provisioned.
Enterprise private provisioning and management tooling are roadmap products under active development.
```

## Pilot metrics

Canonical user journey:

```text
install CYBOU
	-> create alice.cybou (or org.alice.cybou)
	-> secure recovery vault
	-> send encrypted mail to bob.cybou
	-> attach an encrypted document or file
	-> Bob is offline
	-> Bob opens CYBOU later, verifies the mail, retrieves and decrypts the attachment
	-> Bob replies
	-> both restart and retain correct identity/mail/files state
```

This journey is an architecture acceptance test, not only a marketing story:

- `RootPublication` submit-to-finality latency;
- successful later synchronization by recipients who were offline;
- publication discovery and private-index rebuild efficiency;
- decrypt/authentication failures;
- recipient-key/package failures;
- serialized `RootPublication` size and chunk count;
- history growth and node storage resource usage;
- client CPU/RAM/network use;
- Identity key rotation success (`IdentityRotate`);
- support burden and administrative feedback;
- user retention and task completion across Mail and Files without operator intervention.

Pilot success is repeated exchange of mail, secure file sharing, clean-machine
recovery, and continued operational usage by the organization.
