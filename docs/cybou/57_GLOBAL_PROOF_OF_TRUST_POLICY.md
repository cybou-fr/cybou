# 57 — Global Proof of Trust policy

## Purpose

PoT is a deterministic anti-abuse/resource-allocation mechanism for AccountID activity.

It is not social scoring and does not evaluate external personal behavior.

## Global account inputs

```text
System Balance
Account Age
Clean Protocol History
Valid Network Activity
Protocol Violations
```

## Deterministic epoch

All time-based PoT behavior uses a protocol epoch derived from finalized block height.

## Services

One PoT feeds service-specific policies:

```text
Mail
Payments
Identity operations
future Storage
future Backup
future Drive
```

## Mail limits

Example v1 baseline:

```text
25 outgoing MailTx / PoT epoch
```

## Payment limits

PoT can restrict velocity, not ownership.

Receiving funds is not blocked merely because PoT is low.

## Penalties

Objective protocol violations may reduce global PoT.

Service-specific subjective reports should not automatically destroy unrelated privileges.

## No consensus authority

High PoT never turns an account into a validator.

Validator admission is separately operator-controlled.
