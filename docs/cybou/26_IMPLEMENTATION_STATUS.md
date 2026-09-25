# Implementation status

CYBOU is experimental. The canonical product target uses hybrid post-quantum authorization, explicit BFT finality, and one verified state shared by Identity, Email, Wallet, Storage, and Backup. The current DEV node and Qt desktop are still being connected to that target. A development reset will follow the integration of identity, names, operations, blocks, and persistence.

## Implemented core components

- Random stable AccountID, independent of mnemonic and keys.
- 24-word recovery phrase encoding and recovery-derived Ed25519 + ML-DSA-65 root keys.
- Independent Ed25519 + ML-DSA-44 device keys and versioned RecoveryKeyID and DeviceKeyID commitments.
- Portable encrypted CYBV2 vault with Argon2id and AES-256-GCM, durable create-only save, and reopen verification.
- Canonical account creation with anti-Sybil work and hybrid root/device proofs of possession.
- Bounded device registry with add, revoke, root rotation, independent device nonces, and activation numbers that prevent replay after a key is re-added.
- Canonical identity-registry and monetary-state snapshots with a domain-separated state root.
- Account creation that moves the onboarding bonus from OnboardingPool to SystemBalance.
- Device-authorized payments with deterministic fees, overflow checks, and atomic nonce/balance updates on candidate state.
- Versioned AccountCreate and Payment wire encodings, operation IDs, and a candidate block executor that routes four fee units as three Security plus one Onboarding.
- Validator-set validation, hybrid validator signatures, BFT finality certificates, and a core consensus engine with explicit finality.
- Canonical operations, name registry, block execution, state store, and standalone authority/observer sync are connected to the native node runtime.
- The DEV network definition commits to the active name rules. The CLI derives genesis validator keys from the same secret used by the producer; the desktop loads the verified network file.
- Desktop identity creation uses a random AccountID, confirmed 24-word phrase, and durable CYBV2 vault before AccountCreate. Clean-machine restore resolves the RecoveryKeyID from verified state and submits a root-authorized DeviceAdd before activating the new device.
- Native and desktop `.cybou` claiming durably save an encrypted local claim before NameCommit, then perform work and NameReveal; only finalized ownership is displayed as the primary name.
- Four distinct PQ validator keys can be committed to a deterministic DEV genesis, but the current producer remains single-validator Authority Mode. A durable signing high-water mark prevents a validator from signing the last height again after restart; full BFT lock/vote recovery remains open.
- Canonical AuthorityNode, NodeRuntime, MailService, and WalletService smoke suites are back in the native test target. Mail payload encryption works when given the correct mail public key; the current identity registry does not publish one, so `SendMail` fails closed before submission. Full historical integration coverage still needs restoration.
- A separate native P2P session layer exchanges bounded HELLO/PING/PONG, finalized-block requests, and canonical operation submissions over persistent TCP sockets. HELLO advertises block-serving and operation-acceptance capabilities, which are checked before use. An outbound peer manager uses the runtime's NetworkID and finalized status, tracks up to eight explicit peers, rejects duplicates and wrong-network peers, removes peers that fail health checks, bounds TCP connection attempts to five seconds, and commits retrieved blocks only after canonical verification. The DEV producer exposes an optional CYP2 listener with up to eight concurrent inbound sessions; `p2p-probe`, `p2p-sync`, and `p2p-submit` exercise it. `p2p-follow` keeps a headless observer syncing from one endpoint; `p2p-follow-peers` tries up to eight explicitly listed endpoints and fails over after transport loss. NodeRuntime owns a persistent peer session when explicitly configured, and Qt can use that path for sync and operation submission with DEV environment settings. The default bootstrap endpoint remains CYB1 until the remote CYP2 listener is deployed. Operation acknowledgments are distinct from finality. Peer discovery, operation gossip, and block gossip remain open.

## Integration still required

- Complete password change, vault lock and reauthentication, device management, and recovery when the account already has eight active devices.
- Complete Mail confidentiality with independent X25519 and ML-KEM keys, usable recipient discovery, encrypted local mailbox storage, and historical sender-key authorization evidence.
- Finish Qt wallet and Mail flows against the canonical identity and encryption profiles.
- Run independent validators with durable crash recovery and verify finality under production topology.
- Finish operator, release, and treasury signing integration under the PQ key policy.
- Implement distributed Object Storage and Backup before large attachments and mass-scale Mail.
- Remove obsolete runtime paths, names, files, and documentation before the DEV reset. No compatibility decoder or automatic state/vault import is planned.

## Current network boundary

The available development producer runs one validator in Authority Mode (`f=0`). Four equal-weight validators are required to claim tolerance of one Byzantine fault. The bounded DEV transport and compiled bootstrap endpoint are integration tools, not a production peer-to-peer network.

The development network can be reset. Do not treat DEV identities, balances, validator keys, or network state as production assets.
