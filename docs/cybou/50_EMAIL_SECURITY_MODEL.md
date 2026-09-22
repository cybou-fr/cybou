# 50 — CYBOU Email security model

## Protected assets

- plaintext subject/body;
- device private keys;
- CEKs;
- account signing keys;
- recipient relationship metadata where practical;
- authenticity of sender/mail commitment;
- integrity of finalized registration.

## Trust boundaries

Validators and ordinary nodes may see:

- protocol transaction structure;
- ciphertext;
- fees;
- public consensus metadata;
- minimum recipient-discovery metadata required by the design.

They must not receive plaintext or content keys.

## Threats

### Chain metadata analysis

Because MailTx is consensus-visible, metadata privacy is a primary design problem.

Avoid placing human-readable sender/recipient names and plaintext subject information on chain.

### Malformed encrypted content

Text-only v1 sharply reduces parser attack surface.

Do not support active HTML.

### Replay / duplicate MailTx

MailID and transaction/state rules must detect invalid duplicate/replay behavior.

### Recipient-key substitution

Device encryption key packages must be authenticated by AccountID/device authority.

### Consensus rewrite / conflicting mail history

BFT finality and validator safety rules protect finalized registration ordering.

### Old ciphertext retention

Encrypted MailTx may be retained by archive nodes indefinitely.

E2EE must assume ciphertext can remain publicly available forever.

This strengthens the importance of PQ/T confidentiality.

## No Mailbox/relay trust assumption

CYBOU Email v1 has no Email-specific mailbox or relay service.

Offline reception comes from synchronizing chain/state.

## Local endpoint risk

If a recipient device is compromised after decryption, blockchain encryption does not protect the local plaintext copy.

Local mailbox protection remains a client security responsibility.
