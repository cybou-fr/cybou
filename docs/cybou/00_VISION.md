# 00 — Vision

CYBOU is a commercially operated European sovereign identity and communication network, designed in France.

Identity is the security root for user services. Name, Wallet, Mail, and Files
are capabilities of one identity, not separate user accounts or authorization
systems. CYBOU Email is the first user-facing product. Beta includes Files and
Object Storage-backed encrypted Mail attachments. Backup is a later application
of that Storage layer, gated on demonstrated product usage and operational
maturity. See `81_BETA_PRODUCT_SCOPE.md` and `86_IDENTITY_SECURITY_SUBSTRATE.md`.

## Product thesis

The product benchmark is familiar productivity UX: Mail should be immediately
understandable to a Gmail user and Files to a Google Drive user, while CYBOU
keeps identity user-owned, Mail end-to-end protected, network state independently
verifiable, and file content encrypted across distributed storage. These are
interaction references, not visual or branding templates. The concrete
contracts live in `82_MAIL_UI_UX.md`, `83_STORAGE_UI_UX.md`, and
`84_PRODUCT_DESIGN_SYSTEM.md`.

A CYBOU user owns:

```text
identity
keys
devices
verified state
```

not an account inside a provider database.

## Public positioning

```text
CYBOU — European sovereign identity and communication network, designed in France.
```

French:

```text
CYBOU — réseau européen souverain d'identité et de communication, conçu en France.
```

## Ownership and decentralization

CYBOU is not a DAO, community treasury or foundation-owned protocol.

```text
Commercial ownership:
    CYBOU owner/operator

Network operation:
    decentralized / independently operable

Failure objective:
    the network must be able to continue without
    a mandatory CYBOU cloud or central data service
```

Commercial ownership does not create a master key over user funds or E2E Email.

## Product sequence

```text
CYBOU foundation
    Identity
    verified state
    BFT finality
    native economic layer

First user-facing product / Beta requirement
    CYBOU Email + Files + Object Storage-backed encrypted attachments

Post-Beta applications
    Backup
```

CYBOU does not attempt to launch all services at once. One identity powers
Mail, Files, and Wallet. Storage-backed CYBOU Email and the Files product form
the Beta product. Backup follows Beta after demonstrated need and operational
maturity. “Drive-like” describes a usability reference for Files, not a second
CYBOU service or post-Beta milestone.

## Technology trajectory

```text
Bitcoin-derived C++ full-node base
    -> independent CYBOU network
    -> AccountID + optional .cybou identity
    -> Balance + System Balance
    -> global Proof of Trust
    -> E2E encrypted consensus-registered CYBOU Email
    -> bounded-history state sync
    -> BFT explicit finality
    -> independent validators
    -> cooperative encrypted Object Storage for Beta Mail attachments
    -> Files (Beta product surface)
    -> Backup (post-Beta)
```

## Email-first

Email is asynchronous-first:

- recipient may be offline;
- recipient offline availability is natural because mail is consensus-registered;
- organizations already understand email;
- CYBOU Email v1 does not depend on SMTP.

The native asset is infrastructure for metering scarce resources, resisting
abuse, funding validator operation and onboarding. It is not a prerequisite
purchase for a new user.

## End-to-end encryption

All CYBOU-native mail content is E2E encrypted.

Validators, full nodes, archive nodes and future Storage operators do not receive plaintext content keys.

The target key-establishment profile is HPKE with a PQ/T hybrid KEM, initially targeting X25519 + ML-KEM-768 subject to final standards/implementation review.

## Sovereignty

Sovereignty requires:

- no mandatory foreign identity provider;
- no mandatory foreign email provider;
- client/device-controlled decryption authority;
- independently verifiable full-node state;
- no permanent operator master-decryption key;
- no operator ability to seize ordinary user `Balance`;
- network continuity without a mandatory central CYBOU cloud.
