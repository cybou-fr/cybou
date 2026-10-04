# 38 — CYBOU sovereignty definition

CYBOU sovereignty means users retain control of Identity secrets, balances,
decryption, and application state without a mandatory foreign SaaS control
plane. It does not mean the initial network is decentralized: block finality
uses one CYBOU-operated genesis-authorized PoA finalizer, and that operator can
censor or stop progress.

## Cryptographic authority separation

Network ownership and operational block finality are separate cryptographic domains:

```text
Network Ownership != PoA Operation

Network Owner:
    Offline Network Private Key
    Sole authority to sign the immutable genesis specification at network creation
    Never online, never used by runtime

Network Operator (Central Authority):
    Genesis-authorized PoA key P
    Finalizes canonical blocks independently
    Cannot create new networks or sign a genesis specification
```

They may belong to the same company or person, but are separate cryptographic powers.

## Required properties

```text
user-authorized Balance operations
user-held Identity recovery and decryption keys
independent full-node validation
transparent single-operator PoA trust boundary
no mandatory central mailbox cloud
no universal decryption key
no arbitrary operator Balance debit
encrypted content on independent storage providers
```

Mail and Files are product experiences over generic RootPublication and the
shared encrypted chunk tree. Recipients can be offline; clients discover
finalized publications and retrieve admitted encrypted chunks after reconnect.

Sovereignty does not require DAO or foundation ownership. The project must
describe operator finality and storage-provider availability as separate trust
domains.
