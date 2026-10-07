# 38 — CYBOU sovereignty definition

CYBOU is a sovereign cybersecurity, communications and data infrastructure engineered
at the protocol level around an open-source Full Node trust core, designed in France.

Sovereignty means users and organizations retain cryptographic control over Identity
secrets, authorization, decryption, and application state without a mandatory foreign
SaaS control plane. It does not mean the public reference network is decentralized:
block finality on DEVNET uses one CYBOU-operated genesis-authorized PoA finalizer,
and that operator can censor or stop progress.

## Product verticals and deployment models

The shared open-source CYBOU core enables two distinct deployment verticals:

```text
                           CYBOU
                             │
                     Shared CYBOU Core
                 (protocol, Full Node, crypto,
                 storage, state verification)
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
      CYBOU Public                      CYBOU Enterprise
   (One shared network)             (Private CYBOU networks)
            │                                 │
   Reference deployment              Organization trust domain
   France-first public IP policy     Custom admission (VPN, LAN, CIDR)
   CYBOU-operated PoA                Organization-controlled PoA
   Community ecosystem               Enterprise tooling & SLA support
```

### Public network policy vs protocol capability

France-only admission is a deployment policy of the public CYBOU network, not a
limitation of the CYBOU protocol.

The public reference network admits inbound/outbound peers only from French public IP
space (failing closed if Geo data is missing). A private Enterprise CYBOU network
governed by an organization may configure its own network policy:
- corporate VPN only;
- internal LAN or air-gap enclave;
- multi-site CIDR allowlists (e.g. France, Germany, international branches);
- explicit infrastructure allowlists without geographic constraint.

## Cryptographic authority separation

Network ownership and operational block finality are separate cryptographic domains:

```text
Network Ownership != PoA Operation

Network Owner:
    Offline Network Private Key
    Sole authority to sign the immutable genesis specification at network creation
    Never online, never used by runtime

Network Operator (Central Authority / Organization IT):
    Genesis-authorized PoA key P
    Finalizes canonical blocks independently
    Cannot create new networks or sign a genesis specification
```

In an Enterprise private network, both the Network Owner key and the PoA finalizer key
are created and held exclusively by the adopting organization. In the public network,
they are operated by CYBOU.

## Required properties

```text
user-authorized Balance and service operations
user-held Identity recovery and decryption keys
independent full-node validation of blocks and state transitions
transparent single-operator PoA trust boundary
no mandatory central mailbox cloud or SaaS control plane
no universal decryption key in the CYBOU protocol
no arbitrary operator Balance debit
encrypted content stored across verifiable chunk providers
clean-machine Identity, Mail and Files recovery from vault alone
```

Mail and Files are product surfaces over generic `RootPublication` and the
shared encrypted chunk tree. Recipients can be offline; clients discover
finalized publications and retrieve admitted encrypted chunks after reconnect.

Sovereignty does not require DAO or foundation ownership. The project must
describe operator finality, storage-provider availability, and network admission
as distinct, well-defined trust domains.
