# 56 — Owner / Operator model and decentralized resilience

## Business model

CYBOU is a privately/commercially owned service.

It is not a DAO, foundation or community-owned protocol.

The owner/operator may own:

- brand/domain;
- software/IP;
- official release infrastructure;
- company treasury;
- Operator Authority;
- one or more operator-run validators.

## Current authority model

Validator admission is operator-controlled.

A validator becomes active only through an explicit finalized operator-authorized protocol transition.

## Key separation

Operator Authority, Operator Validator, Release Signing and Treasury keys are separate security domains.

Preferred Operator Authority operational protection:

```text
2-of-3
```

This is business key security, not decentralized governance.

## Operational decentralization

The mature network should continue processing already-valid protocol state if the owner's infrastructure is temporarily unavailable, provided enough active validators remain.

## Important distinction

```text
operator-independent operation
!=
operator-independent governance
```

CYBOU aims for the first.

Normal validator-set authority remains with the commercial operator.

## Long-term outage risk

If the Operator Authority disappears permanently, the active validator set cannot evolve forever under the normal model.

Therefore mature production needs an explicitly documented emergency succession/recovery mechanism.

That mechanism must not create hidden everyday community governance.

## Sale of business

A business transfer can move:

- Operator Authority custody;
- release-signing authority;
- operator validator infrastructure;
- domain/trademark/IP;
- company treasury.

User Balance and E2E mail remain cryptographically governed protocol/user state, not arbitrary company database assets.
