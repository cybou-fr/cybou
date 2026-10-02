# 57 — Identity Authority

Every Identity has three canonical account values in PoA-finalized blockchain
state: spendable `Balance` in CYBOU, non-transferable `System Balance` in CYBOU,
and non-transferable `Authority` in AUTH. `AccountState.authority` is committed
by the state root. AUTH is excluded from the 100 billion CYBOU supply.

## Genesis and transitions

`GenesisAllocation.authority` may assign initial AUTH to a RecoveryKeyID. The
matching AccountCreate claims the allocation once and copies that AUTH into
its AccountState. Ordinary AccountCreate starts with zero AUTH. IdentityRotate
preserves the account and its AUTH.

Balance-to-System Balance locks move CYBOU only. Age, activity, fees, storage,
and System Balance do not implicitly create AUTH. A future change to AUTH
requires an explicit protocol state transition.

## Validation eligibility

```text
validation_eligible(identity) :=
    latest_finalized_state.accounts[identity].authority > 1,000,000 AUTH
```

An eligible Identity may sign advisory Validation attestations. Provisional
state never changes eligibility. AUTH grants no PoA finalization, voting,
stake weight, resource allocation, or CYBOU redemption.

There is no AuthorityIndex, local AuthorityPolicy, validator registry,
ValidatorSet, NodeID binding, liveness/storage evidence, reward/penalty system,
resource budget, reservation, grant, ticket, or per-I/O accounting.
