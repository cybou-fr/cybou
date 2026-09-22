# 58 — CYBOU circulation simulation v0.10

This document checks whether the selected integer-token model is numerically plausible.

It is not a market-price forecast.

## Frozen inputs for v0.10

```text
MAX_SUPPLY          = 100,000,000,000 CYBOU
decimals            = 0
Onboarding Pool     = 25,000,000,000 CYBOU
Welcome Grant       = 6,000 CYBOU
standard small MailTx modeling unit ~= 1 CYBOU
new-account ceiling = 25/day
```

## Why 6,000 Welcome CYBOU

A normal-budget design point of 15 MailTx/day gives:

```text
15 * 365 = 5,475
```

Therefore:

```text
6,000 - 5,475 = 525 CYBOU reserve
```

or about 9.6% headroom.

A user who constantly operates near the 25/day security ceiling is a heavier user and may need to move additional CYBOU:

```text
Balance -> System Balance
```

## Gross onboarding capacity before recycling

| Welcome Grant | Full grants from 25B pool |
|---:|---:|
| 5,000 | 5,000,000 |
| **6,000** | **4,166,666** |
| 12,000 | 2,083,333 |

The 6,000 choice gives more than four million full grants even if not one fee returns to onboarding.

## Required recycling at mature growth

For approximate steady state:

```text
required_recycle_share
    ~= annual_new_users * WelcomeGrant
       / (active_users * annual_fee_per_active_user)
```

If average Email use is 10 MailTx/day:

| Annual active-user growth | Required onboarding recycle share |
|---:|---:|
| 5% | 8.2% |
| 10% | 16.4% |
| 20% | 32.9% |
| 30% | 49.3% |
| 50% | 82.2% |

If average use is 15/day:

| Annual active-user growth | Required onboarding recycle share |
|---:|---:|
| 5% | 5.5% |
| 10% | 11.0% |
| 20% | 21.9% |
| 30% | 32.9% |
| 50% | 54.8% |

This shows why the 25B bootstrap pool matters: explosive early growth does not need to be funded entirely from same-year fees.

## 10-year bootstrap examples

Assumptions:

- constant new-user acquisition each year;
- arrivals spread approximately uniformly through the year;
- no churn;
- 25% of modeled Email fees returns to onboarding;
- this simplified model ignores other service fees, price, lost keys and market behavior.

| New users/year | Avg deliveries/day | Recycle | Active after 10y | Minimum pool | Pool after 10y |
|---:|---:|---:|---:|---:|---:|
| 100,000 | 10 | 25% | 1,000,000 | 23.04B | 23.56B |
| 100,000 | 15 | 25% | 1,000,000 | 23.70B | 25.84B |
| 500,000 | 10 | 25% | 5,000,000 | 15.18B | 17.81B |
| 500,000 | 15 | 25% | 5,000,000 | 18.48B | 29.22B |
| 1,000,000 | 10 | 25% | 10,000,000 | 5.36B | 10.62B |
| 1,000,000 | 15 | 25% | 10,000,000 | 11.95B | 33.44B |

Interpretation:

- 100k–500k new users/year is easy for this bootstrap model.
- 1M new users/year remains numerically viable in the modeled 10-year window at 25% recycling.
- faster growth requires either higher recycling, other fee sources, reserve transfers, a smaller grant, or a revised allocation.
- the model must be re-run with real telemetry before production economic constants are declared irreversible.

## Why 10B is rejected

With the same 25% onboarding allocation:

```text
10B total supply
-> 2.5B onboarding
-> only 416,666 full 6,000-CYBOU grants before recycling
```

That is too tight for the intended scale of a general European network with indivisible CYBOU.

## Why 100B is acceptable

100B gives:

- integer fee granularity;
- a 25B onboarding working pool;
- >4.16M grants before recycling;
- enough room for owner, validator/service and strategic pools;
- no need for decimal CYBOU.

## What this simulation does not prove

It does not yet model:

- CYBOU fiat price;
- payment fees;
- Storage/Backup fees;
- churn;
- inactive accounts;
- permanently lost Balance;
- average remaining System Balance;
- validator/service operating costs;
- abuse/Sybil loss rate;
- market liquidity.

Those are required before final production economics.


## v0.14 fee-router note

The 25% onboarding recycle share is unchanged.

The former 40% generic Network Service bucket no longer exists in v1.

Current routing remains:

```text
75% Validators / Security
25% Onboarding
```

Therefore all onboarding-capacity calculations based on 25% recycling remain valid.
