# 69 — Mail evidence bundle

CYBOU Email should support exportable cryptographic evidence for a finalized MailTx.

## EvidenceBundle V1 specification

Implemented in `src/cybou/evidence.{h,cpp}`:

```text
MailEvidenceBundleV1 {
    version: uint8_t = 1
    network_id: uint256
    mail_operation: AuthorizedOperationV1 (payload = MailOpV1)
    block_header: CybouBlockHeaderV1 {
        version: uint8_t
        parent_block_id: uint256
        height: uint64_t
        operations_root: uint256
        resulting_state_root: uint256
    }
    inclusion_proof: OperationInclusionProofV1 {
        operation_index: uint32_t
        operation_hashes: vector<uint256>
    }
    finality_certificate: BftFinalityCertificateV1
    sender_authorization: AccountAuthorizationV1
}
```

### Verification flow

1. **Network & Operation integrity**:
   - `network_id` matches verifier network;
   - `mail_operation.payload` is a valid `MailOpV1`.
2. **Supplied sender key**:
   - `mail_operation.signature` verifies with `sender_authorization.authorization_descriptor` over `ComputeUserOperationDigest(network_id, sender_id, nonce, payload)`.
   - The bundle does not prove that this key was canonical for the sender at the block height.
3. **Block transaction inclusion**:
   - `inclusion_proof` verifies that `SerializeProtocolOperation(mail_operation)` is at `operation_index` in `operation_hashes`, and `ComputeOperationsRootFromHashes(operation_hashes) == block_header.operations_root`.
4. **BFT finality certificate**:
   - `ComputeBlockHeaderId(block_header) == finality_certificate.block_id`;
   - `block_header.height == finality_certificate.height`;
   - `VerifyFinalityCertificate(finality_certificate, validator_set, network_id)` succeeds.

## Sender key at historical height

It is not enough to show that a signing key belongs to an AccountID today.

The evidence must prove that the sender signing key was authorized for that AccountID at the MailTx finalization height.

This preserves verification after later key rotation or revocation.

The current V1 bundle does not yet meet that requirement. It contains no
historical account-state or key-transition proof. Its inclusion proof carries
all operation hashes, so its size is linear in the number of block operations.
Treat the bundle as finalized inclusion plus signature evidence until a
historical authorization proof and compact authenticated operation tree exist.

## Content commitment

Do not publish a simple hash of predictable plaintext.

Preferred pattern:

```text
salt = cryptographically random

ContentCommitment =
    H(
      "CYBOU-MAIL-CONTENT-V1"
      || salt
      || canonical_plaintext_mail
    )
```

The salt remains inside E2E encrypted mail content unless the recipient later chooses to disclose it.

## Why salted commitment

For short/predictable plaintext, a bare plaintext hash can permit dictionary guessing.

A random salt prevents practical offline guessing before voluntary disclosure.

## Later disclosure

If a recipient wants to prove content externally:

```text
plaintext
+ salt
+ MailEvidenceBundle
```

can be checked against:

```text
ContentCommitment
sender signature
block inclusion
BFT finality
signature against the supplied sender key (historical authorization still requires separate proof)
```

## Time semantics

Evidence should distinguish:

```text
client_created_time
consensus timestamp if defined
finalized block height
```

The strongest protocol fact is finalized ordering/height.

Do not claim exact legal trusted time unless the consensus timestamp rules and applicable legal qualification support that claim.
