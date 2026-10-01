# 70 — Account creation and anti-Sybil work

Status: consensus operation and current desktop CVID5 identity path are
implemented. End-to-end clean-machine and operational recovery remain product
readiness work.

Account creation is permissionless. No operator approval, voucher, or central activation is involved. A new AccountID is a random nonzero 256-bit identifier independent of the recovery phrase and signing keys.

## Canonical operation

`AccountCreateOp` contains AccountID, a hybrid Recovery Root and initial Identity authorization, `AccountCreationWork`, and two proofs of possession. The root proof requires Ed25519 and ML-DSA-65; the authorization proof requires Ed25519 and ML-DSA-44. Both proofs cover a domain-separated digest bound to NetworkID, AccountID, and the exact authorization commitment.

The work serialization is 113 bytes and binds NetworkID, AccountID, authorization commitment, height-derived work epoch, and nonce. The complete account creation encoding is 9,334 bytes. Version bytes are part of these wire formats. Consensus rejects missing or invalid signature components.

## Verification and state transition

Full nodes check canonical encoding, network and account bindings, authorization commitment, work difficulty, valid epoch range, both proofs of possession, duplicate accounts and recovery keys, the per-block creation limit, and OnboardingPool solvency. Consensus uses block height and immutable network parameters, never local wall-clock time.

A successful operation atomically registers the Identity, debits the network onboarding bonus from OnboardingPool, and creates the monetary account with zero spendable Balance and the bonus in System Balance. The state change becomes durable only after the genesis-bound PoA finalizer signs the block and each full node verifies the deterministic transition.

## Onboarding and Authority interaction

Account creation remains permissionless and protocol-native. `AccountCreateOp` uses anti-Sybil work bound to NetworkID and AccountID. There is no voucher, operator approval, or central activation.

A successful AccountCreate registers the Identity and monetary account and transfers the configured onboarding value from OnboardingPool to System Balance. It does not mint supply.

Automatic onboarding credit earns no Authority. Authority policy does not change the account-creation or monetary transition.

## Local creation gate

The portable CYBV2 vault contains the random AccountID and recovery entropy;
the current Identity roles derive from that entropy. It must be durably saved
and authenticated by reopening before broadcast. Keep tests covering
save-before-broadcast, phrase confirmation, and clean-machine restore. DEV
already uses the current protocol.

DEV, Beta, and Mainnet use separate economic parameters and genesis states. Beta balances do not carry to Mainnet.
