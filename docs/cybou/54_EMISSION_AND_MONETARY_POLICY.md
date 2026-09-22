# 54 — CYBOU emission and monetary policy

## v0.10 monetary baseline

CYBOU uses a **fixed maximum supply**.

```text
MAX_SUPPLY = 100,000,000,000 CYBOU
decimals   = 0
minimum unit = 1 CYBOU
```

There is no perpetual base emission in the v0.10 monetary design.

## Why 100 billion

CYBOU intentionally has no decimal denomination.

A large integer supply provides fee granularity without fractional token UX.

Ten billion would make the same onboarding/service model much tighter.

At 100 billion, a standard small text MailTx fee can target an intuitive integer amount such as:

```text
1 CYBOU
```

without forcing `0.00000... CYBOU` displays.

## Genesis allocation

```text
Owner / Developer / Operator      25,000,000,000   25%
User Onboarding Pool              25,000,000,000   25%
Network Operations / Validators   20,000,000,000   20%
Product / Ecosystem Reserve       15,000,000,000   15%
Strategic Reserve                 15,000,000,000   15%
-----------------------------------------------------------
MAX SUPPLY                       100,000,000,000  100%
```

## Owner / Developer / Operator allocation

The owner/operator allocation is:

```text
25,000,000,000 CYBOU
```

There is no artificial founder vesting requirement in the protocol baseline.

CYBOU is a privately owned commercial service, not a community/foundation launch.

The owner/operator allocation is distinct in accounting from operational pools even if the same company initially administers multiple pools.

## Operator validator compensation

The owner/operator is currently the only development validator and is expected to remain an operator validator.

Development-network rewards may be disabled because they have no economic meaning.

On a live multi-validator network, the operator validator may receive normal deterministic validator compensation for verified consensus work.

## User Onboarding Pool

```text
25,000,000,000 CYBOU
```

funds Invite Bonus.

v0.10:

```text
WELCOME_GRANT = 6,000 CYBOU
```

Gross grants available before any recycling:

```text
25,000,000,000 / 6,000
= 4,166,666 full grants
```

with a remainder of `4,000 CYBOU`.

## No mint-on-invite

Creating an AccountID does not mint new CYBOU.

```text
Onboarding Pool
-> System Balance
```

is a transfer inside fixed supply.

## Fee policy

v0.13 recurrent fee routing before Object Storage is:

```text
75%  Validators / Security
25%  Onboarding
```

Integer settlement:

```text
4 CYBOU fees
-> 3 Security
-> 1 Onboarding
```

There is no recurring burn, no generic service-node bucket and no recurring owner tax.

The owner/operator earns its recurring protocol revenue by participating as an eligible validator.

When Object Storage launches, a future version may introduce a separately justified Storage provider share.


## Why fixed supply can work

The same CYBOU can repeatedly circulate:

```text
Onboarding Pool
-> user System Balance
-> protocol fees
-> validators/onboarding recycling
-> later back into onboarding or user circulation
```

Supply is fixed; economic activity is not.

## Mainnet immutability warning

Changing the hard cap after a public production launch would be a major monetary-policy change.

Therefore implementation may use development constants before mainnet, but production genesis must freeze the final 100B cap explicitly and test supply invariants.
