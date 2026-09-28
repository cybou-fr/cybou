> Historical/current-DEV scope: this document describes the BFT, MailTx, validator-set, or indexed-object protocol currently running on DEV. It is superseded for the next-gen target by `POA_FINALITY.md`, `ENCRYPTED_CHUNK_DAG.md`, `ROOT_PUBLICATION.md`, `STORAGE_ADMISSION.md`, and `IDENTITY_DISCOVERY_AND_RECOVERY.md`. Product and UX requirements remain applicable only where they do not conflict with those target documents. No cutover is active yet.
# 81 — CYBOU Beta product scope

Status: canonical product-scope decision. This document defines Beta readiness;
it does not claim that the required services are implemented or authorize a
consensus or wire-format change. See DEC-173 through DEC-194 in
`24_DECISIONS.md` for product scope and the Mail/Files architecture freeze.

## Product boundary

CYBOU Beta requires the complete identity and communication product plus the
Object Storage capability needed to send and retrieve encrypted Mail
attachments:

```text
CYBOU network and BFT finality
Identity, recovery, Identity key rotation, and .cybou names
Wallet and service balance
Hybrid-PQ Mail
Object Storage
Encrypted Mail attachments
```

Mail and Files are not separate cryptographic or storage stacks. They use one
CYBOU Identity, finalized state, and shared encrypted Object Layer. Mail targets
Gmail's familiar core workflows; Files targets Google Drive's familiar file
management workflows. These are interaction references only; CYBOU keeps its
own design and user-owned trust model. DEC-194 and
`spec/mail_files_architecture.yaml` freeze the product architecture.

Files is the Beta file-management product surface, with the familiar
Drive-like workflows described in `83_STORAGE_UI_UX.md`. There is no separate
Drive product milestone. Backup is a post-Beta application of the same Storage
layer and is not a prerequisite for Beta readiness.

Email remains CYBOU's first user-facing product. For Beta, complete Email means
E2E Mail with text and encrypted attachments for one or more `.cybou`
recipients. The initial DEV/Alpha profile remains single-recipient; Beta group
recipient limits and per-recipient KEM capsules are separate protocol gates.
Attachment objects are durably retrievable through CYBOU Object Storage,
including when recipients were offline at send time.

## DEV and Alpha transition

The initial Mail profile remains one-recipient, text-only, with no attachments.
DEV and Alpha may use this profile to integrate and validate identity,
consensus, discovery, and Mail flows before Store is available. This is a
transitional engineering scope; it does not satisfy the Beta Mail requirement.

Do not enable attachment sending in a release until the Store path and E2E
attachment descriptors are interoperable and pass the Beta readiness gates
below. Do not represent validator pre-Store retention as the Beta content
architecture.

## Beta Mail and Storage boundary

```text
Sender client
  -> encrypts the message and attachment descriptors end to end
  -> encrypts, chunks, and uploads each attachment as an opaque Storage object
  -> waits for the required durability threshold
  -> submits first-class MailTx with the encrypted message and commitment
  -> BFT finality

Recipient client, now or later
  -> discovers and verifies finalized MailTx
  -> retrieves referenced encrypted object(s) from Store
  -> verifies integrity and decrypts locally
```

Attachment bytes never enter MailTx, BFT blocks, or consensus state. Each
attachment is an immutable Object Layer object with its own Storage manifest.
The attachment descriptor (ObjectID, manifest commitment, sizes, MIME type,
display name, and object key) lives inside E2E-protected Mail content. MailTx
carries the encrypted envelope plus protocol fields needed for registration,
discovery, and commitments; it carries no Storage topology. Filenames, MIME
types, paths, subjects, content keys, and attachment descriptors stay inside
E2E-protected content. Storage manifests contain only opaque chunk descriptors
and commitments, and providers receive ciphertext only. Exact wire fields and
the interoperable PQ/T key-package profile remain governed by the protocol and
cryptography freeze gates.

## Minimum Storage capability for Beta

The Beta Storage path must provide:

- client-side encryption and opaque object identifiers/commitments;
- bounded chunking and encrypted manifest storage/retrieval;
- distributed placement and an explicit durability contract;
- provider leases and verifiable storage audits;
- detection, repair, and retrieval of damaged or unavailable shards;
- integrity-checked GET/PUT and offline-recipient retrieval;
- accounting sufficient to meter service use and validate provider obligations.

Replication factors, lease periods, audit cadence, repair deadlines, capacity
limits, provider rewards, and Storage fee/accounting parameters are not frozen
by this product-scope decision. Define and validate them before Beta; do not
infer values from the existing 3:1 contribution-to-entitlement target or
change that target here.

## Beta readiness gates

Beta is not ready until all of the following work end to end:

1. A sender creates an E2E message for one or more `.cybou` identities, with
   encrypted attachment objects and durable Storage manifests.
2. The message is finalized while recipients may be offline; after later sync,
   each recipient verifies, retrieves, decrypts, and opens the attachment.
3. Store providers and validators never receive plaintext content, keys,
   filenames, or MIME metadata.
4. Commitment verification, placement audits, provider loss, interrupted
   upload/download, repair, restart recovery, and restoration of Files keys
   after clean-machine Identity recovery pass the defined Storage reliability
   tests.
5. Mail evidence, historical sender authorization, Storage accounting, and
   user-visible pending/failure states are integrated and reviewed.
6. Beta network, economics, independent-validator, release, and operational
   gates are satisfied under their owning documents.

Backup may follow after Beta operational evidence. The Beta onboarding budget
must be calibrated from measured Email and Files/Storage usage. Backup is a
separate post-Beta capacity scenario and is not an input to the Beta budget
freeze. Files is in Beta scope; there is no separate Drive service or
additional Drive calibration category.

## Product-experience gate

Beta readiness includes the user-facing product, not only protocol capability.
Mail and Files must satisfy the canonical interaction contracts in
`82_MAIL_UI_UX.md`, `83_STORAGE_UI_UX.md`, and `84_PRODUCT_DESIGN_SYSTEM.md`.
The end-to-end scenarios in `85_BETA_UI_ACCEPTANCE.md` are part of the Beta
product gate. Gmail and Google Drive are ergonomic references, not visual
templates or endorsements of centralized custody. CYBOU preserves familiar
workflows while keeping identity user-owned, Mail end-to-end protected, and
file content encrypted in distributed Storage.

## Document authority

`81_BETA_PRODUCT_SCOPE.md` owns the Beta product boundary. `11_STORAGE_OBJECTS.md`
owns Storage architecture; `49_EMAIL_E2EE_HPKE_PQ.md` owns Mail confidentiality;
`22_ROADMAP.md` owns milestone order; `24_DECISIONS.md` records frozen product
decisions. `82_MAIL_UI_UX.md` owns normal Mail interaction and state
presentation; `83_STORAGE_UI_UX.md` owns Files/Storage interaction;
`84_PRODUCT_DESIGN_SYSTEM.md` owns shared visual and status vocabulary;
`85_BETA_UI_ACCEPTANCE.md` owns end-to-end product acceptance. Older text-only
or Storage-later plans remain valid only for the initial DEV/Alpha profile or
as historical rationale, as marked in those files.
