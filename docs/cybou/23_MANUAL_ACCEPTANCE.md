# 23 — Manual acceptance v0.14

## Network quarantine
```text
[ ] cybou.exe starts
[ ] separate CYBOU datadir
[ ] no Bitcoin peers/seeds/blocks/UI/URI
```

## Typed protocol operations
```text
[ ] MailTx is an explicit CYBOU operation type
[ ] MailTx is not OP_RETURN/application blob abuse
[ ] canonical serialization deterministic
[ ] oversized MailTx rejected
[ ] priority fee path disabled
```

## Invite Voucher
```text
[ ] unsigned voucher rejected
[ ] expired voucher rejected
[ ] reused voucher rejected
[ ] valid voucher transfers 6,000 CYBOU OnboardingPool -> SystemBalance
[ ] identity creation alone gives no grant
```

## Mail crypto
```text
[ ] content commitment uses random salt
[ ] plaintext subject/body absent from chain
[ ] sender signature verifies
[ ] recipient key package authenticated
[ ] modified ciphertext rejected
[ ] downgrade rejected
```

## Mail finality/offline receive
```text
[ ] Bob offline during send/finality
[ ] Alice MailTx finalizes
[ ] no permanent MailMarker is added to current state
[ ] Bob later discovers relevant block/range
[ ] Bob obtains historical ciphertext
[ ] Bob verifies inclusion/finality/signature
[ ] Bob decrypts locally
```

## Pre-Store retention
```text
[ ] desktop node can prune according to policy
[ ] active validator retains required pre-Store MailTx history
[ ] historical recipient can retrieve ciphertext from validator/archive path
[ ] product does not claim state hash can reconstruct pruned ciphertext
```

## PoT
```text
[ ] epoch derived deterministically from finalized height
[ ] 25 MailTx/epoch enforced for new invited account
[ ] local wall clock/timezone changes do not affect consensus result
[ ] PoT arithmetic is integer/deterministic
```

## BFT
```text
[ ] 1-validator dev mode works
[ ] 2–3 validator integration works
[ ] 4-validator f=1 scenarios simulated
[ ] equal vote weight
[ ] operator-authorized activation/removal is chain-visible
[ ] conflicting finalization safety tests pass
```

## Evidence
```text
[ ] MailEvidenceBundle export
[ ] tx inclusion proof verifies
[ ] finality certificate verifies
[ ] historical sender-key authorization verifies
[ ] plaintext + disclosed salt recomputes ContentCommitment
```
