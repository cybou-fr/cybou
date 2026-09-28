# CYBOU Beta product scope

This document defines the user-facing Mail and Files product boundary. It does
not specify network operations or storage wire formats; those are defined by
the active protocol authority in `AGENTS.md`.

## Product boundary

CYBOU Beta delivers one account-level Identity with two core user surfaces:

- **Mail:** private, encrypted messaging with familiar Gmail workflows.
- **Files:** private file and folder management with familiar Google Drive
  workflows.

The products share one local-first encrypted content substrate and recovery
model. CYBOU keeps its own branding and user-owned trust model. Backup is a
post-Beta application and is not required for Beta readiness.

## Mail

The initial release profile supports one recipient and UTF-8 text without
attachments. The product experience includes Inbox, Sent, Drafts, Archive,
Trash, threads, local labels, unread state, local search, reply, forward,
blocked senders, and `.cybou` contact autocomplete.

Before enabling attachments or multiple recipients, the Beta client must pass
the shared encrypted-content, retrieval, and clean-machine recovery gates. Mail
must distinguish local draft, submitted, finalized, available, and retrievable
states. Offline recipients must be able to retrieve content after reconnecting.

## Files

The Files product includes:

- files and folders, upload and download, rename, move, copy, and trash/restore;
- Recent, Starred, local search, sorting, grid/list views, preview, and storage
  usage;
- asynchronous progress with pause, retry, and interruption recovery;
- immutable version history and identity-based sharing;
- private names, paths, and file metadata; no plaintext search terms leave the
  client.

The client reports availability only after the configured durability condition
is met. Local drafts and private catalogs remain recoverable across restarts.

## Beta readiness

- Clean-machine Identity restore without a previous client database.
- Offline-recipient Mail retrieval and sender authorization verification.
- Mail attachment and Files round-trips over the shared encrypted substrate.
- Measured durability, retry, repair, retention, and provider-loss behavior.
- User-visible distinction between finalized, available, and retrievable data.
- Gmail-familiar Mail and Google Drive-familiar Files workflows that pass the
  acceptance criteria in docs 82–85.
- Support, privacy, security, and operational cost review for a controlled
  pilot.

The target uses a single-operator PoA finalizer. Product language must describe
that centralized trust model clearly and must not imply Byzantine fault
tolerance. See `26_IMPLEMENTATION_STATUS.md` for implementation status and
`25_OPEN_QUESTIONS.md` for remaining gates.
