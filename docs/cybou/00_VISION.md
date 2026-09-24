# 00 — Vision

CYBOU is a commercially operated European sovereign identity and communication network, designed in France.

Identity is the platform primitive. CYBOU Email is the first complete product.
Object Storage, Backup and Drive are later sovereign services, gated on
demonstrated product usage and operational maturity.

## Product thesis

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

First product
    CYBOU Email

Later sovereign services
    Object Storage
    Backup
    Drive
```

CYBOU does not attempt to launch all services at once. Identity and CYBOU
Email form the first complete product. Storage, Backup and Drive are gated on
demonstrated real-world usage and operational maturity.

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
    -> cooperative encrypted Object Storage, after product gates
    -> Backup
    -> Drive
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
