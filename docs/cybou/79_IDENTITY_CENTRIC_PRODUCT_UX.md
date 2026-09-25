# 79 — Identity-centric product UI and UX

Status: product UX contract.

This document defines how CYBOU presents the platform to ordinary users. It
sits above page-specific desktop specifications such as the Identity and Wallet
UX documents. Core and protocol terminology remains authoritative internally,
but the normal product surface is organized around the user's CYBOU identity,
people, actions, and outcomes.

The central rule is simple:

> The user operates an identity and its services. The user does not operate a
> blockchain, validator set, cryptographic suite, peer graph, or state machine.
>
> **Not a blockchain with features — a protected identity with services.**

CYBOU must remain technically transparent and diagnosable, but protocol detail
is progressively disclosed rather than placed in the primary workflow.

## 1. Product model

The primary product object is the user's stable CYBOU identity.

```text
                         stanislav.cybou
                               |
          +--------------------+--------------------+
          |                    |                    |
        Mail                 Wallet               Files
          |                    |                    |
       People              Payments             Sharing
          |                    |                    |
          +--------------------+--------------------+
                               |
                            Backup
                               |
                         Identity security
                               |
                   Recovery / trusted devices
```

`AccountID` remains the permanent protocol identifier, but a finalized
`.cybou` name is the normal human-facing identifier. Services attach to the
same identity; they must not feel like unrelated mini-applications with
separate accounts, addresses, keys, or security models.

Identity-centric does **not** mean social-profile-centric. A `.cybou` name is a
pseudonymous network identity, not a civil identity assertion. CYBOU should not
publish profile data, contact graphs, device metadata, or personal information
merely because an AccountID owns a name.

## 2. Information architecture

The main navigation should be service-oriented and human-readable:

```text
Home
Mail
Files                 when Storage is available
Wallet
Backup                when Backup is available

--------------------------------
stanislav.cybou       identity menu / security
```

The identity is persistent context, not a technical page the user has to visit
before every action. The identity chip in the application chrome should show
the finalized `.cybou` name when available and open the Identity & Security
area.

The Identity & Security area contains recovery, password, devices, name,
security status, and advanced identity details. It is not the normal starting
point for Mail, payments, files, or backup.

Before a `.cybou` name is finalized, the UI may show a neutral temporary label
such as `Your CYBOU identity`. It must not promote the raw AccountID to the
primary product identity merely because a human name is not yet available.

## 3. Home screen

Home answers four ordinary questions:

```text
Who am I?
Am I protected?
Is CYBOU ready to use?
What do I want to do next?
```

The dominant identity card should contain:

```text
stanislav.cybou
Post-quantum protected
Online

[ Share identity ]   [ Copy name ]

Mail        Send CYBOU        Files        Backup
```

The exact set of quick actions is capability-gated. An unavailable service is
not presented as functioning merely because its page exists.

Secondary cards may show recent mail, recent payments, storage/backup status,
or actions requiring attention. Raw block height, validator count, peer count,
NetworkID, OperationID, and cryptographic key material do not belong on Home.

## 4. User-facing language

Normal UI copy should describe intent and outcome rather than the mechanism.

| Protocol/internal term | Normal product language |
| --- | --- |
| AccountID | Identity ID, only when a technical identifier is explicitly needed |
| `.cybou` name claim | Register your CYBOU name |
| NameCommit / work / reveal | Registering your name |
| AccountCreationWork / PoW | Preparing / securing your registration |
| ProtocolOperation | Action / request, or no noun at all |
| OperationID | Hidden; shown in Advanced details |
| BFT finality | Confirmed / Finalized |
| Validator quorum | Hidden; Network security details |
| block height | Hidden; Advanced network details |
| epoch | Hidden |
| nonce / activation nonce | Hidden |
| Recovery Root | Recovery protection / recovery key in advanced security details |
| DeviceKeyID | Device identifier in advanced security details |
| ML-DSA / ML-KEM / X25519 | Post-quantum protection; exact algorithms in Security details |
| peer count | Hidden; Advanced network details |
| NetworkID | Hidden; Advanced network details |
| state root | Hidden |
| commitment | Hidden |
| System Balance | Service balance in the normal UI; protocol name may appear in details |

Technical terminology must not leak into an error message merely because the
core returned a technical error code.

Examples:

```text
Bad:  NameReveal rejected: insufficient commit depth.
Good: Your name is still being secured. CYBOU will continue when it is ready.

Bad:  BFT certificate not available.
Good: Waiting for network confirmation.

Bad:  DEVICE_ADD root nonce mismatch.
Good: Your identity changed on another device. Sync CYBOU and try again.
```

## 5. Progressive disclosure

CYBOU uses three layers of detail.

### Layer A — normal product UI

For nearly all users:

```text
stanislav.cybou
Online / Syncing / Offline
Protected / Attention needed
Sending / Sent / Confirmed
Protecting / Protected
Registering name / Name ready
This device / Other devices
```

No protocol knowledge is required.

### Layer B — Identity & Security

For a user who wants to understand and control security:

```text
Recovery phrase backup
Vault password
Lock / unlock
Trusted devices
Revoke device
Recovery status
Post-quantum protection
Encrypted export
Name ownership
```

This view can explain security concepts, but should still avoid forcing the
user to understand wire formats, nonces, consensus rounds, or key sizes.

### Layer C — Advanced / Diagnostics

For developers, operators, support, and technically curious users:

```text
AccountID
NetworkID
DeviceKeyID / RecoveryKeyID
finalized height
validator set
peer count and endpoints
operation IDs
cryptographic suites
sync diagnostics
logs
protocol versions
```

Hiding these values from the main UI does not mean removing them from the
product. CYBOU should remain inspectable without making inspection the default
experience.

## 6. First-run experience

The first screen has two clear choices:

```text
Create identity
Restore identity
```

There is no separate wallet creation, mail account creation, node setup, or
key-generation wizard.

### Create identity

The user experience is:

```text
Choose a local password
        ↓
Write down 24 recovery words
        ↓
Confirm selected words
        ↓
Creating your CYBOU identity…
        ↓
Choose yourname.cybou
        ↓
Ready
```

Internally CYBOU may be generating random AccountID material, deriving hybrid
keys, writing CYBV2, performing anti-Sybil work, broadcasting operations,
waiting for finality, committing a name, doing name work, and revealing it.
Those steps are implementation details.

A compact progress screen may show human phases such as:

```text
Protecting your identity
Registering with CYBOU
Confirming
Securing your name
Ready
```

An optional `Technical details` expander can expose the exact underlying phase
for diagnostics.

The recovery phrase screen must clearly distinguish the two secrets:

```text
Password
Protects this CYBOU installation on this device.

Recovery words
Recover your CYBOU identity if this device is lost.
```

The two must never be presented as interchangeable.

### Restore identity

The normal restore flow is:

```text
Enter 24 recovery words
        ↓
Choose a new local password
        ↓
Finding your identity…
        ↓
Authorizing this device…
        ↓
stanislav.cybou restored
```

Do not show `RecoveryKeyID lookup`, `DeviceAdd`, root nonce, or finality
certificate in the normal flow.

If the client is not sufficiently synchronized to locate the identity, say:

```text
CYBOU is still syncing securely. Your identity can be restored when the
verified network state is ready.
```

not `RecoveryKeyID not found`.

## 7. `.cybou` names

A finalized `.cybou` name is the default address for human interaction.

Users should normally:

```text
send mail to anna.cybou
send CYBOU to anna.cybou
share a file with anna.cybou
recognize a contact as anna.cybou
```

rather than copy and paste 64-character AccountIDs.

Name registration UI has one primary input:

```text
Choose your CYBOU name
[ stanislav               ] .cybou
```

Availability before finality is advisory. The UI may say `Available` as a
pre-check but must not say `Yours` until finalized ownership exists in verified
state.

Normal progress should be summarized as:

```text
Checking name
Securing name
Confirming ownership
stanislav.cybou is yours
```

The commit/work/reveal mechanism remains completely invisible unless Advanced
details are opened.

## 8. People instead of addresses

All service entry points should prefer `.cybou` names.

When a name is resolved, the confirmation surface should prominently show the
name and, where useful, a locally stored contact label or avatar. AccountID is
secondary technical evidence accessible through details.

For sensitive actions such as payments, the confirmation screen should make
identity ambiguity difficult:

```text
Send 250 CYBOU
To: anna.cybou

[ Confirm ]
```

An optional details disclosure can show the resolved AccountID before signing.
The product should never force users to verify long hexadecimal strings as a
routine safety mechanism.

## 9. Mail experience

Mail should feel like private mail, not like submitting a transaction.

Normal states:

```text
Draft
Sending
Sent
Confirmed
Failed — Retry
```

A recipient is entered as `name.cybou`. Encryption, hybrid signatures,
MailTx construction, P2P propagation, fees, evidence bundles, discovery tags,
and BFT finality are handled by core.

The message view may expose a simple security indicator:

```text
End-to-end encrypted
Post-quantum protected
Network-confirmed
```

A `Security details` panel may show cryptographic/evidence information for a
user who explicitly asks for it.

Attachments, when Storage is available, should be presented as ordinary
attachments. The user should not have to understand chunks, CIDs, redundancy,
providers, or placement to attach and receive a file.

## 10. Wallet experience

Wallet actions are identity-to-identity:

```text
Send CYBOU to anna.cybou
Receive at stanislav.cybou
```

Do not present a crypto-wallet address workflow as the primary experience.

Normal balances should be understandable without protocol education:

```text
Available balance
Service balance
```

`Service balance` is the human-facing name for protocol `System Balance`. Its
explanation is outcome-oriented:

> Used automatically for CYBOU services and network protection. It cannot be
> sent or withdrawn.

The protocol term `System Balance` may be shown in Advanced details and
technical documentation.

A payment lifecycle is simply:

```text
Sending
Confirmed
```

not mempool, operation relay, block inclusion, certificate verification, or
nonce advancement.

## 11. Files and Storage

Storage should appear as a user capability of the same identity, not as a P2P
storage administration console.

Normal concepts:

```text
My files
Shared with me
Share with anna.cybou
Available offline
Protected
```

Normal users do not manage shard IDs, replication sets, proofs, leases,
placement, repair, or provider nodes.

Technical storage health may appear in Advanced diagnostics. User-facing
health is expressed as outcomes such as:

```text
Protected
Uploading
Repairing protection
Unavailable — retrying
```

## 12. Backup

Backup should answer one question:

> Can I recover my data and identity after losing this computer?

Normal status:

```text
Backup protected
Last verified: today 18:42
```

Restore begins from the user's identity and recovery process. The UI should not
make the user reconstruct protocol topology or storage placement before a
restore can start.

## 13. Devices

The user sees devices, not public keys.

```text
This PC                This device
Laptop                 Active
Old laptop             Revoked
```

Device labels are local/user-facing metadata unless a future protocol explicitly
requires otherwise.

Primary actions:

```text
Add device
Revoke device
Rename device locally
```

A destructive revoke confirmation explains the consequence in human language:

> This device will no longer be able to act as your CYBOU identity after the
> change is confirmed.

`DeviceKeyID`, activation nonce, hybrid signature suite, and authorization
operation are Advanced details.

## 14. Security center

Identity & Security should communicate a small number of understandable
properties:

```text
Identity                    stanislav.cybou
Recovery words              Backed up
Vault                        Locked / Unlocked
Password                     Set
Devices                      2 active
Post-quantum protection      On
Network confirmation         Verified
```

Warnings must be actionable:

```text
Recovery words not confirmed
[ Back up now ]

New device waiting for confirmation
[ View ]
```

Avoid generic red security banners whose only remedy is reading logs.

Exact algorithms belong under `Security details`, for example:

```text
Recovery authorization      Ed25519 + ML-DSA-65
Device authorization        Ed25519 + ML-DSA-44
Mail confidentiality        X25519 + ML-KEM-768
Vault                        Argon2id + AES-256-GCM
```

The normal user only needs the summary `Post-quantum protection: On`.

## 15. Network state

The normal application has three useful network states:

```text
Online
Syncing
Offline
```

A fourth state, `Attention needed`, may be used when the local verified state
cannot safely progress.

Do not use raw peer count as the primary network-health indicator. One healthy
peer may be enough to progress and many peers do not themselves prove safety.

When syncing, show user-impact rather than protocol trivia:

```text
Syncing securely…
Mail and payments will be available when verified state is current.
```

Advanced diagnostics may show peer count, endpoints, finalized height,
validator set, sync source, and transport details.

## 16. Confirmation and finality

The UI must never invent success before canonical state says it happened.

Optimistic feedback is allowed only as a pending state:

```text
Sending…
Registering…
Revoking…
Protecting…
```

Final language is reserved for verified finality:

```text
Confirmed
Name ready
Device revoked
Backup verified
```

This preserves the core/desktop truth boundary while keeping consensus jargon
out of the normal UI.

## 17. Error model

Every core error presented to the user belongs to one of five product
categories:

```text
Needs user action
Temporary network problem
Still waiting for confirmation
Conflict with finalized state
Local security/storage problem
```

The UI maps technical error codes into these categories and gives one clear
next action.

Examples:

```text
Name already finalized elsewhere
→ "That CYBOU name has already been registered. Choose another name."

Insufficient service balance
→ "Your service balance is too low for this action."

Network unavailable
→ "CYBOU is offline. Your request has not been sent."

Vault authentication failure
→ "The password is incorrect or the identity vault is damaged."
```

Raw error codes remain available through diagnostics and logs.

## 18. No blockchain cosplay

CYBOU must not imitate cryptocurrency-wallet conventions merely because some
protocol components resemble a chain.

Avoid making these primary UI concepts:

```text
block explorer
transaction hash
mempool
mining
confirmations count
wallet address
seed-derived account number
gas
validator dashboard
peer topology
```

Some may exist as operator or diagnostic tools, but they do not define the
consumer product.

The desired mental model is closer to:

```text
one secure identity
+ private communication
+ payments
+ encrypted files
+ backup
```

than to a blockchain client.

## 19. Advanced mode

Advanced mode is the safety valve that lets CYBOU remain technically honest
without making the product technical by default.

Suggested path:

```text
Settings
  -> Advanced
      -> Identity details
      -> Network diagnostics
      -> Cryptography
      -> Storage diagnostics
      -> Logs
```

Advanced mode must never unlock protocol-invalid actions. It exposes
information and diagnostics, not shortcuts around consensus or security
invariants.

## 20. Cross-service continuity

The same identity must visually follow the user across the product.

If the active identity is `stanislav.cybou`, Mail, Wallet, Files and Backup
should all clearly operate as that identity without asking the user to select
keys, accounts, addresses, networks, or cryptographic profiles again.

Changing device, restoring from recovery words, rotating recovery authority,
or revoking a device must preserve the visible identity:

```text
stanislav.cybou
mail history
balances
shared-file identity
backup ownership
```

where protocol rules allow the underlying data to be restored.

This continuity is one of the main product advantages of CYBOU and should be
felt everywhere in the UX.

## 21. Visual hierarchy

Normal screens should follow this hierarchy:

```text
1. Human identity / person / object
2. User action
3. Outcome / status
4. Security assurance when relevant
5. Technical evidence only on demand
```

A screen is probably too technical if the first thing a user sees is a hash,
key, NetworkID, block height, validator count, peer count, operation kind, or
cryptographic algorithm.

## 22. UX acceptance tests

A release candidate satisfies the identity-centric contract when a new user can
complete the following without understanding blockchain or cryptography:

```text
create an identity
write down recovery words
register a .cybou name
send mail to another .cybou name
send CYBOU to another .cybou name
add or revoke a device
restore identity on a clean machine
understand whether the app is online, syncing, or offline
understand whether an action is pending or confirmed
```

During those flows, the user is never required to manually handle AccountID,
NetworkID, DeviceKeyID, OperationID, nonce, epoch, validator quorum, block
height, state root, commitment, ML-DSA, ML-KEM, X25519, or peer endpoints.

A technical user must still be able to inspect those values through Advanced
or Security details.

## 23. Relationship to other documents

This document owns the **product language, information hierarchy, and
progressive-disclosure rules**.

`78_IDENTITY_DESKTOP_UX.md` owns the concrete create/restore/name security flow.
`72_DESKTOP_WALLET_UI.md` owns wallet behavior and protocol balance invariants.
`73_CORE_DESKTOP_CONTRACT.md` owns the truth boundary between verified core
state and GUI state.
`02_ARCHITECTURE.md` owns system architecture.

If a page-specific document exposes protocol detail that this document marks as
Advanced-only, the protocol behavior remains unchanged but the default UI must
follow this identity-centric presentation contract.
