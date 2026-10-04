# CYBOU vision

CYBOU is an experimental European identity and private communication platform,
designed in France. One account-level Identity is the security root for Mail,
Files, and Wallet. A portable recovery phrase restores the account; a device
installation is not a protocol identity.

## Product thesis

Mail should feel familiar to Gmail users and Files to Google Drive users while
keeping CYBOU branding, local control, end-to-end encryption, and independently
verifiable network state. These products use one shared encrypted content
substrate. Backup follows Beta after product use and operating costs are
understood.

The product contracts are `82_MAIL_UI_UX.md`, `83_STORAGE_UI_UX.md`,
`84_PRODUCT_DESIGN_SYSTEM.md`, and `81_BETA_PRODUCT_SCOPE.md`.

## Ownership and network trust

CYBOU is commercially operated. The active protocol has one offline Network Key
owning the signed genesis, one genesis-authorized Central Authority PoA finalizer,
and independently validating full nodes.
This is a centralized finality trust model; CYBOU does not claim Byzantine fault
tolerance. The client verifies finalized history rather than trusting a hosted
gateway.

The design aims to avoid a mandatory CYBOU cloud for user decryption and
recovery. Encrypted content remains client-readable from portable recovery
material and public network data, subject to provider availability.

## Product sequence

```text
Identity, recovery, names, and Wallet
    -> private text Mail
    -> shared encrypted chunks and finalized storage admission
    -> Mail attachments and Files
    -> controlled Beta
    -> Backup (post-Beta)
```

The initial Mail profile is one recipient and text-only. Files and attachments
remain gated on content retrieval, durability, recovery, and familiar desktop
UX acceptance.

## Technology trajectory

```text
independent CYBOU full nodes
    -> account-level Identity and .cybou names
    -> genesis-authorized single-operator hybrid-PQ PoA finality
    -> generic RootPublication
    -> streaming encrypted ordered chunk tree
    -> finalized chunk admission and retrieval
    -> private Mail and Files client indexes
```

The active protocol and remaining integration gates are maintained in
`AGENTS.md` and the active PoA, encrypted chunk-tree, RootPublication,
storage-admission, and Identity-recovery documents.

## Security and economics

Mail, Files, and recovery secrets remain end-to-end protected. Network
operators and storage providers do not receive plaintext content keys.
Identity recovery, Identity authorization, Identity KEM, PoA, Release Signing,
and Treasury remain separate key roles, with no classical-only production
signature fallback.

The native asset meters scarce resources and funds onboarding. A new user does
not need to purchase tokens to create an Identity. Product trust and finality
claims must accurately disclose the single-operator PoA model.
