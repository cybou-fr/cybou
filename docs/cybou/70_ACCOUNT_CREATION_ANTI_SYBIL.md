# 70 — Account creation and anti-Sybil work

Status: the canonical consensus operation is implemented. Desktop vault creation and recovery remain to be integrated before the DEV cutover.

Account creation is permissionless. No operator approval, voucher, or central activation is involved. A new AccountID is a random nonzero 256-bit identifier independent of the recovery phrase and signing keys.

## Canonical operation

`AccountCreateOp` contains AccountID, a hybrid Recovery Root and initial device authorization, `AccountCreationWork`, and two proofs of possession. The root proof requires Ed25519 and ML-DSA-65; the device proof requires Ed25519 and ML-DSA-44. Both proofs cover a domain-separated digest bound to NetworkID, AccountID, and the exact authorization commitment.

The work serialization is 113 bytes and binds NetworkID, AccountID, authorization commitment, height-derived work epoch, and nonce. The complete account creation encoding is 9,334 bytes. Version bytes are part of these wire formats. Consensus rejects missing or invalid signature components.

## Validation and state transition

Validators check canonical encoding, network and account bindings, authorization commitment, work difficulty, valid epoch range, both proofs of possession, duplicate accounts and recovery keys, the per-block creation limit, and OnboardingPool solvency. Consensus uses block height and immutable network parameters, never local wall-clock time.

A successful operation atomically registers the identity, debits the network onboarding bonus from OnboardingPool, and creates the monetary account with zero spendable balance and the bonus in SystemBalance. The state change becomes durable only after BFT finality.

## Local creation gate

The portable CYBV2 vault must contain the random AccountID, recovery entropy, and independent device secret. It must be durably saved and authenticated by reopening before broadcast. The current desktop identity service still uses the older seed-based keystore; it does not yet satisfy this gate. Do not reset DEV or claim completed desktop onboarding until the vault, phrase confirmation, clean-machine restore, and finalized identity flow are integrated.

DEV, Beta, and Mainnet use separate economic parameters and genesis states. Beta balances do not carry to Mainnet.
