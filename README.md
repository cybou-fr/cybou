# CYBOU

**One identity. Private communication. Your data under your control.**

CYBOU is an open-source project building an identity-centered platform for
private messaging and user-controlled files. It is experimental software, not
a public mail or cloud-storage service. The desktop client and network are
under active development.

## Current status

The active protocol is deployed on the experimental DEV network. DEV uses a
genesis-bound, hybrid-signature Proof of Authority (PoA) finalizer operated by
CYBOU. Full nodes independently verify blocks and state transitions. This is a
single-operator trust model: it is **not BFT**, and the network does not claim
Byzantine-fault-tolerant finality.

The protocol and storage substrate are in place, but the user-facing Mail and
Files product is not complete. In particular, CYBOU is not yet ready for public
or production use. DEV state and tokens are experimental and have no production
value.

## Architecture

- **One account-level Identity.** A portable encrypted vault stores a stable
  AccountID and recovery entropy. The 24-word phrase restores Identity keys;
  it does not by itself restore Mail or Files content.
- **Separate key roles.** Identity recovery, Identity authorization, Identity
  key agreement, PoA, Release Signing, and Treasury are distinct. Required
  production signing paths use Ed25519 together with the designated ML-DSA
  profile; there is no classical-only fallback.
- **One content publication operation.** `RootPublication` is the only
  application-content operation. Mail, Files, and future Backup schemas are
  encrypted client data, not separate consensus object types.
- **One encrypted chunk tree.** Ordered ROOT/INDEX/DATA chunks are encrypted
  before storage and addressed by the full BLAKE3-256 hash of their stored
  ciphertext. Recipient key capsules do not publish recipient AccountIDs.
- **Proof-based provider admission.** A provider accepts a chunk only when a
  valid inclusion proof ties it to a finalized publication. Finality authorizes
  storage; it does not prove availability or durability.
- **Client-owned views.** Clients are expected to scan finalized publications
  and rebuild local Mail/Files indexes. Local databases are caches, not the
  source of protocol identity.

DEV pins the X-Wing / HPKE post-quantum draft-05 profile. It is DEV-only and is
not an external audit or a production security claim. Beta and Mainnet need
separately reviewed profiles and independent genesis parameters.

## Implementation and next work

The current source includes hybrid Identity operations and recovery vaults,
finalized `.cybou` name claims, genesis-bound PoA with a durable equivocation
safety halt, `RootPublication`, the encrypted chunk tree, local provider
admission, and CYP2 block/chunk transport.

The main product work is to connect these pieces into complete flows:

1. Build and submit encrypted Mail and Files publications from the desktop.
2. Scan publications, open recipient capsules, retrieve chunks, and rebuild
   Inbox, Sent, and Files views after restart or clean-machine recovery.
3. Complete provider selection, retry, durability measurement, retention,
   repair, and provider-loss handling.
4. Finish Gmail-familiar Mail and Google Drive-familiar Files UX, then validate
   the integrated experience on DEV.
5. Establish Beta operating costs and security evidence. Backup is post-Beta.

Read [the implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md) for
the maintained list of implemented components and open gates.

## Build

See [INSTALL.md](INSTALL.md) for prerequisites and build instructions, including
the Windows MinGW/Ninja path. The project uses C++20, CMake, Qt 6 for the
desktop client, and the repository's native dependencies.

## Protocol and product documentation

- [Vision](docs/cybou/00_VISION.md)
- [Architecture](docs/cybou/02_ARCHITECTURE.md)
- [Protocol and product roadmap](docs/cybou/22_ROADMAP.md)
- [Implementation status](docs/cybou/26_IMPLEMENTATION_STATUS.md)
- [PoA finality and trust model](docs/cybou/POA_FINALITY.md)
- [Identity and `.cybou` names](docs/cybou/10_IDENTITY_NAMES.md)
- [Identity vault and recovery](docs/cybou/76_IDENTITY_VAULT_RECOVERY.md)
- [RootPublication](docs/cybou/ROOT_PUBLICATION.md)
- [Encrypted chunk tree](docs/cybou/ENCRYPTED_CHUNK_TREE.md)
- [Storage admission](docs/cybou/STORAGE_ADMISSION.md)
- [Mail product and UX contract](docs/cybou/82_MAIL_UI_UX.md)
- [Files product and UX contract](docs/cybou/83_STORAGE_UI_UX.md)
- [Beta acceptance criteria](docs/cybou/85_BETA_UI_ACCEPTANCE.md)

## Contributing and security

- [Contributing](CONTRIBUTING.md)
- [Security policy](SECURITY.md)
- [License](COPYING)

Do not use DEV keys, balances, or data as production assets. Report security
issues through the process in `SECURITY.md`.

© 2026 Stanislav Saveliev. Designed in France.
