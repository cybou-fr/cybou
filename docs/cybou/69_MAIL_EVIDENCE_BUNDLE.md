# 69 — Mail evidence bundle

The native `MailEvidenceBundle` exports a finalized MailTx, its block header, an operation inclusion proof, the BFT finality certificate, and the sender device public key. The wire format has its own version byte.

## Verification available now

`VerifyMailEvidenceBundle` checks NetworkID, the hybrid device signature against the supplied key, operation inclusion under the block operations root, the block ID and height, and the finality certificate against the supplied validator set. The inclusion proof currently carries all operation hashes in the block, so its size grows with the number of operations.

The mail payload contains a salted, domain-separated content commitment. The salt is inside the encrypted message and can be disclosed later with the plaintext. The current `VerifyDisclosedMailContent` interface checks a supplied commitment; a complete independent plaintext-and-salt disclosure verifier remains to be integrated.

## Historical authorization gap

The bundle contains the sender device key but no proof that this key was authorized for the sender AccountID at the block's finalized height. Verification against the supplied key alone cannot establish historical authorization after rotation or revocation. A complete evidence claim needs a verified historical identity-state proof or an equivalent authenticated transition history.

Until that proof exists, present the bundle as finalized operation inclusion plus a signature under the supplied device key, with the historical authorization limitation stated explicitly.
