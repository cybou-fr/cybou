# 42 — Competitive positioning

CYBOU is a sovereign cybersecurity, communications and data infrastructure designed
in France around an open-source Full Node P2P core.

It exposes familiar Mail and Files workflows over user-held Identity keys, generic
`RootPublication`, and a shared encrypted content layer. Familiarity is a usability
asset, not the strategic category.

## Core differentiation

### Why not ordinary SaaS?
Traditional SaaS platforms (Google Workspace, Microsoft 365, Dropbox) operate a
mandatory centralized control plane: they generate or store account identities, manage
encryption keys or hold access to decrypted content, and enforce proprietary tenancy.
CYBOU is engineered at the protocol level without a mandatory foreign SaaS control plane:
Identity keys remain strictly with the user/organization, encryption is performed client-side,
and state transitions are validated independently by every Full Node.

### Why not just an encrypted messenger or email plugin?
Standalone encrypted email tools (PGP, S/MIME plugins) or consumer messengers lack an
integrated trust infrastructure: they depend on third-party mail servers, centralized
directory servers, or external cloud storage. CYBOU integrates:
```text
network-native Identity
+ encrypted asynchronous communication (Mail)
+ distributed encrypted storage (Files)
+ independent Full Node state verification
+ deterministic network finality
+ verifiable chunk replication and recovery
+ hybrid classical and post-quantum cryptography
```
into a single, coherent protocol stack.

### Why CYBOU Enterprise?
While **CYBOU Public** operates a shared reference network with France-first public IP
admission, **CYBOU Enterprise** enables organizations to deploy their own private CYBOU
networks with:
- an independent cryptographic trust domain (unique NetworkID and offline-signed genesis);
- organization-controlled PoA finality authority;
- organization-chosen node and storage infrastructure (on-premise, private cloud, air-gap);
- tailored network admission policies (corporate VPN, internal LAN, CIDR allowlists);
- commercial enterprise tooling under development (fleet management, admin console, SSO/HSM connectors, SLA support).

## Security claims discipline

Do not claim:

```text
compatible with standard Internet SMTP/IMAP email (CYBOU Mail is protocol-native)
zero metadata (network observations and inclusion commitments exist)
quantum-proof or unhackable (experimental hybrid PQ compositions; no absolute claims)
legal notary status
ANSSI certified or government approved
RGPD certified or NIS 2 compliant by software design alone
total autonomy or absolute cryptographic isolation
zero single point of failure (distinct StorageIds do not prove independent failure domains)
```

Transparent disclosure of the single-operator PoA on DEVNET and the active development
stage is mandatory across all competitive materials.
