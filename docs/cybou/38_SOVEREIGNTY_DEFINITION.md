# 38 — CYBOU sovereignty definition

CYBOU sovereignty means users retain control of Identity secrets, balances,
decryption, and application state without a mandatory foreign SaaS control
plane. It does not mean the initial network is decentralized: block ordering
uses one CYBOU-operated genesis-bound PoA finalizer, and that operator can
censor or stop progress.

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
