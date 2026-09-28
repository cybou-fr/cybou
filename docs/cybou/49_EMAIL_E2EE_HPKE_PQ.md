# 49 — Mail E2E encryption profile

Status: target profile for one-recipient, text-only DEV Mail. Mail remains disabled for confidential sending until a vetted HPKE backend and end-to-end client flow are integrated and reviewed. This specification defines account-level keys; there is no device set or activation semantics.

## Key ownership

Identity derives Recovery signing, Authorization signing, and X-Wing KEM material from recovery entropy using distinct labels. Mail never reuses signing keys for encryption. One finalized Identity KEM package is current for each AccountID/key_epoch. Its commitment binds NetworkID, AccountID, key_epoch, and canonical package bytes. The client must verify the finalized state snapshot and package commitment before encryption or decryption.

## Message construction

Initial Mail is one recipient, UTF-8 text subject/body, and no attachments. Generate a random 32-byte content-encryption key (CEK), a random 12-byte content nonce, and a random 32-byte MailID. Encrypt the canonical ProtectedMail plaintext with the approved AEAD; the ciphertext includes its authentication tag. No plaintext or permanent per-mail object is stored in consensus state.

Encapsulate the CEK to the recipient's current X-Wing public key using the pinned DEV HPKE base-mode suite: KEM `0x647a`, HKDF-SHA256 `0x0001`, and ChaCha20Poly1305 `0x0003`. Wrap exactly the 32-byte CEK. One HPKE encapsulation and wrapped CEK are included for the recipient AccountID and key_epoch.

## Canonical `MailEnvelopeV1`

The binary format is little-endian unless a field explicitly says otherwise. Unknown versions, suite IDs, invalid lengths, trailing bytes, zero MailID/package commitment, mismatched expected key_epoch, and envelopes over the strict MailTx size bound are rejected.

```text
version                    u8
kem_id                     u16be  0x647a
kdf_id                     u16be  0x0001
aead_id                    u16be  0x0003
mail_id                    32 B
recipient_state_height     u64le
recipient_state_root       32 B
content_nonce              12 B
recipient_key_epoch        u64le
recipient_package_commit   32 B
encapsulation              1120 B
wrapped_cek                48 B
content_ciphertext_length  u32le
content_ciphertext         length B
```

The finalized state height/root freezes which recipient Identity snapshot was used. The recipient package commitment and key_epoch make the capsule unambiguous across IdentityRotate transitions. The schema contains no recipient key ID, device ID, activation nonce, or recipient array.

## HPKE context and associated data

Use the pinned HPKE base-mode `SetupBaseS` / `SetupBaseR` flow. `info` is the exact ASCII domain `CYBOU/MAIL/HPKE-INFO/V1` followed by NetworkID, MailID, sender AccountID and sender key_epoch, recipient AccountID and recipient key_epoch, and the three suite IDs in network byte order. The exact serialized `info` bytes are included in interoperability vectors.

AEAD associated data is the exact serialized envelope header from `version` through `recipient_package_commit`, followed by NetworkID, sender AccountID, and sender key_epoch. This binds content to the chosen finalized recipient snapshot, sender, network, and suite. Do not add local timestamps, display names, MIME metadata, or unreviewed fields.

## Authorization and evidence

The outer MailTx is a first-class protocol operation. The sender Authorization key signs the canonical Mail payload commitment through `IdentityOperationCoordinator`, using the current shared Identity nonce and key_epoch. Deterministic size-aware fees apply; priority fees are disabled. The payload commitment includes a random salt and a domain separator. No mail is broadcast if package lookup, finality/state-root verification, HPKE, signing, serialization, size, fee, or nonce reconciliation fails.

Mail evidence must include transaction inclusion proof, BFT finality certificate, historical sender Authorization key authorization at the included height, and salted/domain-separated content commitment. A supplied public key plus a valid signature alone does not prove historical authorization. Until the historical Identity transition proof is implemented, evidence must state this limitation and cannot be represented as complete sender identity proof.

## Local mailbox and lifecycle

The local client owns Inbox, Sent, Drafts, read state, and message indexes. Validators retain canonical finalized MailTx history for the pre-Store retention policy; desktop nodes may prune. There is no permanent consensus Mail object. Rotation changes the account's future recipient package; it does not retroactively alter finalized messages. The client must preserve any local decryption material needed for previously received messages before replacing the vault.

## Enablement gate

Before enabling Send or receive, integrate a vetted implementation matching the pinned suite; draft and publish interoperability vectors for exact `info`, AAD, envelope bytes, encapsulation and decapsulation; verify recipient state and BFT certificates; verify historical sender authorization; enforce the strict maximum MailTx size and deterministic fee; durably store local encrypted mailbox data; and complete the Gmail/Google Drive-inspired desktop composer and inbox flows. No fallback to plaintext or another key profile is permitted.
