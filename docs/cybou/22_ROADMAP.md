# CYBOU protocol and product roadmap

This roadmap describes the single active target in `main`: generic encrypted
content over RootPublication and a streaming ordered chunk tree, finalized by a
genesis-bound single-operator PoA signer. The PoA trust model is centralized
and does not provide Byzantine fault tolerance. DEV has completed the
coordinated reset to this protocol; routine work continues on that network.
Any later incompatible format requires a separate explicit cutover decision.

## Protocol substrate delivered

The authority is `POA_FINALITY.md`, `ENCRYPTED_CHUNK_TREE.md`,
`ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`,
`IDENTITY_DISCOVERY_AND_RECOVERY.md`, and `spec/poa_chunk_tree.yaml`.

Canonical formats, Identity authorization, generic RootPublication, the
streaming encrypted chunk tree, content-addressed provider admission, CYP2
transport, and genesis-bound PoA finality are implemented on active DEV.
Legacy `StorageObject`, the dedicated Mail envelope/service placeholder, and
the separate storage key-ring modules are physically removed. Do not add
runtime compatibility or a dual operation decoder.

Remaining integration and Beta readiness work is tracked in
`26_IMPLEMENTATION_STATUS.md`. PoA trust remains centralized. A finalized
publication authorizes storage; it does not prove that providers retain chunks.

## Product direction

Mail and Files are required product surfaces with familiar Gmail and Google
Drive workflows under CYBOU branding. They share one Identity and one private
encrypted content substrate. Mail starts with one recipient and text-only
messages. Files, attachments, sharing, and Backup remain gated on the common
chunk storage and recovery path. Backup is post-Beta.

### Alpha product gate

- Create and restore an Identity on a clean installation.
- Claim and use a `.cybou` name.
- Send encrypted text to an offline recipient and retrieve it after the
  recipient reconnects.
- Rebuild local Mail indexes across restarts and verify sender authorization.
- Independently verify finality and state on a full node.
- Explain the single-operator PoA trust model clearly in product UX.
- Build Mail publication/inbox scanning and Files catalog flows over the
  shared encrypted chunk tree; reconstruct content without a prior local DB.

### Beta product gate

- Store and retrieve finalized encrypted chunks with a measured durability
  threshold, retry, repair, and interruption recovery.
- Support Mail attachments and Files catalogs over the shared private chunk
  graph.
- Provide Gmail-familiar Mail workflows and Google Drive-familiar Files
  workflows, including progress, failure, retry, preview, search, and recovery.
- Complete the applicable product and UX acceptance criteria in docs 79–85.
- Validate clean-machine recovery without relying on a prior client database.

### Controlled pilot

Run a controlled pilot with 20–100 users for 8–12 weeks after the Beta gates
pass. Measure Mail and Files usage, offline retrieval, recovery, storage repair,
support burden, and history growth. Keep Backup post-Beta and disclose the
single-operator trust model.

## Economic and release gates

- Keep DEV, Beta, and Mainnet parameters and genesis states separate.
- Size Beta onboarding from measured integrated Mail and Files/Storage costs.
- Freeze the Mainnet onboarding bonus only after aggregate Beta operations.
- Keep the release, treasury, Identity, and PoA key roles separate.
- No production signature path may fall back to classical-only signatures.
