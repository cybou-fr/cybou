# Implementation status

CYBOU is experimental. The canonical product target uses hybrid post-quantum authorization, explicit BFT finality, and one verified state shared by Identity, Email, Wallet, Storage, and Backup. The standalone DEV node and native Qt desktop use the canonical CYBOU runtime; the desktop remains an observer role, with opt-in multi-peer CYP2 networking. A development reset will follow the integration of identity, names, operations, blocks, and persistence.

Beta product scope requires Object Storage-backed encrypted Mail attachments;
neither distributed Store nor the end-to-end attachment flow is implemented.
The current text-only Mail profile is limited to DEV/Alpha integration. See
`81_BETA_PRODUCT_SCOPE.md` for Beta readiness criteria.

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
- The desktop runs as an observer. Setting `CYBOU_DEV_VALIDATOR` fails closed until the desktop shares the complete validator networking and consensus lifecycle with `cybou-node`.
- The desktop build is CYBOU-native: inherited Bitcoin Qt UI sources, locale catalogs, translation tooling, and the BitcoinApplication test harness have been removed. Desktop smoke coverage uses only `cybou_qt`, `cybou_node`, and Qt Test.
- Configured multi-peer desktop sync now checks every connected peer when one reports `UP_TO_DATE`, isolates remote protocol failures from the network worker, and backs off a dropped bad peer. Wallet payment and one-way lock submission run off the Qt thread; received-mail fee display uses the active network's canonical protocol parameters.
- Native and desktop `.cybou` claiming durably save an encrypted local claim before NameCommit, then perform work and NameReveal; only finalized ownership is displayed as the primary name.
- Four distinct PQ validator keys can be committed to a deterministic DEV genesis, and the node serves N>1 validator clusters over CYP2 with a four-process smoke test as a CI regression gate. The CBS2 signing journal persists round and lock state so a restarted validator can safely resume an unfinished height; legacy CBS1 journals conservatively abstain at that height.
- Canonical AuthorityNode, NodeRuntime, MailService, and WalletService smoke suites are back in the native test target. The local X25519 mail-encryption helper is an incomplete prototype, not the required hybrid profile, and finalized incoming mail is not decrypted through it. The identity registry does not publish recipient mail keys, so `SendMail` fails closed before submission. Do not claim production Mail confidentiality until the standardized PQ/T profile, key publication/discovery, encrypted mailbox storage, and historical authorization evidence are integrated.
- `CybouNodeService` owns shared runtime initialization, desktop observer networking, and authority block-feed/CYP2 listeners, consensus scheduling, peer gossip, and historical catch-up. With explicit DEV CYP2 settings, desktop discovers peers, keeps up to eight outbound sessions, syncs verified blocks across peers, and fans out recent blocks; an inbound listener is separately opt-in. Its default bootstrap remains CYB1 until the remote CYP2 endpoint is available. The desktop remains observer-only in validator role; both native desktop and `cybou-node` use the same node service.
- A separate native P2P session layer exchanges bounded HELLO/PING/PONG, finalized-block requests, and canonical operation submissions over persistent TCP sockets. HELLO advertises block-serving and operation-acceptance capabilities, which are checked before use. An outbound peer manager uses the runtime's NetworkID and finalized status, tracks up to eight peers, rejects duplicates and wrong-network peers, removes peers that fail health checks, bounds TCP connection attempts to five seconds, and commits retrieved blocks only after canonical verification. CYP2 peer discovery exchanges bounded numeric endpoint lists; configured peers are bootstrap seeds, while discovered endpoints remain in-memory routing hints. The DEV producer exposes an optional CYP2 listener with up to eight concurrent inbound sessions; `p2p-probe`, `p2p-sync`, and `p2p-submit` exercise it. `p2p-follow` keeps a headless observer syncing from one endpoint; `p2p-follow-peers` tries up to eight explicitly listed endpoints and fails over after transport loss. `p2p-submit-peers` tries multiple explicit operation admission endpoints, and `operation-status` locates an OperationID in complete local finalized history. `CybouNodeService` owns desktop peer discovery, retry, multi-peer catch-up, block fanout, and an optional inbound listener; the default desktop bootstrap still uses CYB1 until the remote CYP2 service is deployed. Operation acknowledgments are distinct from finality. Operation and block gossip remain bounded initial implementations.

## Integration still required

- Complete password change, vault lock and reauthentication, device management, and recovery when the account already has eight active devices.
- Complete Mail confidentiality with independent X25519 and ML-KEM keys, usable recipient discovery, encrypted local mailbox storage, and historical sender-key authorization evidence.
- Finish Qt wallet and Mail flows against the canonical identity and encryption profiles.
- Run independent validators with durable crash recovery and verify finality under production topology.
- Finish operator, release, and treasury signing integration under the PQ key policy.
- Implement distributed Object Storage and encrypted Mail attachments before Beta; Backup is post-Beta.
- Remove obsolete runtime paths, names, files, and documentation before the DEV reset. No compatibility decoder or automatic state/vault import is planned.

## Current network boundary

The development node runs one validator in Authority Mode (`f=0`) by default and already supports N>1 `serve` with an explicit peer list; a four-process validator cluster is exercised by a CI smoke gate. Four equal-weight validators are required to claim tolerance of one Byzantine fault. The bounded DEV transport and compiled bootstrap endpoint are integration tools, not a production peer-to-peer network. Discovered peers are untrusted routing hints only: explicit validator endpoints keep gossip priority, and generic discovery never defines consensus connectivity.

The development network can be reset. Do not treat DEV identities, balances, validator keys, or network state as production assets.
