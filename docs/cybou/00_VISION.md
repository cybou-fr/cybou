# 00 — Vision

CYBOU is a commercially operated European sovereign peer-to-peer service and network, designed in France.

The first user-facing application is **CYBOU Email**.

## Public positioning

```text
CYBOU — European sovereign email on a peer-to-peer network, designed in France.
```

French:

```text
CYBOU — messagerie électronique souveraine européenne sur réseau pair-à-pair, conçue en France.
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
    -> cooperative encrypted Object Storage
    -> Backup
    -> Drive
```

## Email-first

Email is asynchronous-first:

- recipient may be offline;
- recipient offline availability is natural because mail is consensus-registered;
- organizations already understand email;
- CYBOU Email v1 does not depend on SMTP.

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
