# Identity and `.cybou` names

Status: canonical protocol target. Core cryptography, vault, registry, account creation, payment, state, and the initial desktop create/restore path exist. Mail encryption and name claiming still need product integration.

## Identity layers

| Layer | Meaning | Rotation |
| --- | --- | --- |
| AccountID | Random nonzero 256-bit permanent identifier | Never |
| Primary `.cybou` name | Human-facing alias bound to AccountID | No transfer or recycling |
| Recovery Root | Hybrid authorization recovered from 24 words | Yes |
| Device authorization | Bounded operational keyset with independent nonces | Add or revoke |
| Mail encryption keys | Separate recipient confidentiality keys | Yes |

AccountID is independent of every mnemonic and public key. A versioned RecoveryKeyID maps to the stable AccountID. Rotating keys preserves balances, names, and finalized Mail history. A name is a pseudonymous alias, not a civil identity assertion.

The Recovery Root requires **Ed25519 and ML-DSA-65** signatures. Each
operational device requires **Ed25519 and ML-DSA-44** signatures. Both
components are mandatory; missing, malformed, or failed components fail
closed. Device encryption capability is separate from signing: the DEV
draft-05 X-Wing KEM public key is published in a canonical package, and its
context-bound commitment is stored for each active device. The CVID4 vault
stores the matching 32-byte seed. Mail and Files do not consume this capability
until their application gates pass. Beta and Mainnet keep it disabled by
default. See [Identity KEM publication](89_IDENTITY_KEM_PUBLICATION.md) for
the frozen profile and coordinated cutover.

The current authorization descriptor has a fixed 3331-byte canonical form:
version `02`, root suite `01`, 32-byte Ed25519 root public key, 1952-byte
ML-DSA-65 root public key, device suite `01`, 32-byte Ed25519 device public
key, and 1312-byte ML-DSA-44 device public key. The domain-separated
commitment covers this exact encoding. Any recipient KEM capability requires a
separately reviewed identity/wire-format change; do not append unreviewed key
fields to this descriptor.

The identity registry binds each active device signing key to its finalized
KEM package commitment. The desktop treats a vault as ACTIVE only when both
the signing key and the package recomputed from its local X-Wing seed match
the current activation. Application use of this capability remains gated by
the Mail and Files integration profiles.

## Devices and recovery

At most eight devices may be active per account. Root-authorized add, revoke, and recovery rotation operations bind NetworkID, AccountID, root nonce, and target key ID. A new device proves possession of both key components. Each device has its own nonce and activation number; a re-added key receives a new activation number so signatures from its previous activation cannot be replayed.

Revocation changes future authorization without erasing received ciphertext or historical signatures. Historical authorization proofs must remain available for Mail evidence. On a clean machine, the phrase derives RecoveryKeyID; verified state locates AccountID, and the root authorizes a new device.

## Names

The primary example is `stanislav.cybou`. A label is 5–32 lowercase ASCII bytes and follows the grammar and reserved-name rules in the [name registry](77_CYBOU_NAME_REGISTRY.md). Names finalize through commit, work, and reveal. The desktop flow is specified in the [identity UX contract](78_IDENTITY_DESKTOP_UX.md).

## Network cutover

The canonical state joins monetary accounts, the identity registry, validator set, and name ownership under one state root. Account creation, payments, names, finalized blocks, persistence, and the initial desktop create/restore path share that state. PQ Mail confidentiality, broad transport, independent validators, and remaining desktop security controls still require integration. The DEV reset discards obsolete state and vaults; there is no compatibility decoder or automatic import.
