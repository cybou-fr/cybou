# 54 — CYBOU monetary policy

The maximum supply is fixed. There is no perpetual base emission.

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals = 0
```

Account creation does not mint tokens. A successful AccountCreate operation
atomically transfers the network's configured onboarding bonus from
OnboardingPool to the new account's System Balance. Protocol fees route three
parts to Security and one part to Onboarding in four-unit integer batches.

DEV, Beta, and Mainnet have separate genesis allocations and economic
parameters. Beta balances do not carry to Mainnet. Mainnet onboarding values
remain unset until integrated Mail and Files operating data is measured.
