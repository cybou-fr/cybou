# 40 — France/EU regulatory readiness

This document is an engineering/compliance planning aid, not legal advice.

CYBOU should obtain professional French/EU legal advice before public commercial launch.

## 1. Cyber Resilience Act (CRA)

CYBOU is software with security-sensitive network functionality.

As of 11 September 2026, CRA reporting obligations for manufacturers regarding actively exploited vulnerabilities and severe security incidents are already in application. Full CRA application is scheduled for 11 December 2027.

Therefore security operations begin early, not at v1.

Required project capabilities:

```text
SECURITY.md
security contact
vulnerability intake
triage severity
coordinated disclosure process
incident log
dependency inventory / SBOM target
supported-version policy
signed releases
security update mechanism
CRA reporting playbook
```

Do not wait until certification/production to build these.

## 2. French cryptography controls

In France, use of cryptographic means is free, but provision/import/intra-EU transfer/export can be subject to declaration or authorization depending on the operation and technical characteristics.

CYBOU therefore needs a pre-commercial cryptography classification/export workstream.

Before public binary distribution:

```text
inventory crypto functionality
identify supplier/importer role
assess ANSSI declaration/authorization regime
assess "grand public" classification eligibility if relevant
assess dual-use/export obligations for non-EU distribution
document supported distribution territories
```

This is one reason for Europe-first commercial rollout instead of global-first.

## 3. CYBOU Email / DSA boundary

Private interpersonal messaging between a finite number of recipients is not treated as an "online platform" merely because messages pass through an online service.

However public groups/open channels can materially change the DSA analysis.

Therefore Email v1 intentionally excludes:

```text
public open channels
public content feeds
unlimited public publishing
```

Any later public/community features require a fresh legal/product review.

## 4. Communications-service classification

CYBOU Email may fall within European rules applicable to number-independent interpersonal communications services.

Before commercial launch, obtain legal analysis covering:

```text
European Electronic Communications Code
ePrivacy/confidentiality obligations
national French implementation
lawful process obligations
security/incident duties
```

Do not assume P2P architecture removes provider obligations.

## 5. GDPR/privacy

CYBOU should minimize operator-visible personal data by design.

Principles:

```text
data minimization
local/client-side encryption
minimal chain-visible mail metadata
bounded retention
purpose limitation
device/key lifecycle transparency
privacy-preserving logs
```

Encrypted data can still be personal data depending on context. "Ciphertext" does not automatically eliminate GDPR obligations.

## 6. Native CYBOU asset / MiCA

Do not make the early project dependent on public token economics.

Current early policy:

```text
CYBOU asset exists for protocol/dev needs
NO ICO
NO public token sale
NO exchange-listing objective
NO speculative marketing
```

Before any public economic launch:

```text
classify the asset under MiCA
assess issuer obligations
assess white-paper obligations
assess CASP activities
assess custody/exchange/payment features
assess AML/KYC consequences where applicable
```

Since France's prior transitional crypto-services period ended on 1 July 2026, do not rely on legacy PSAN transitional assumptions.

## 7. Storage later

When general distributed Storage launches, obtain separate analysis for:

- hosting/intermediary-service roles;
- unlawful-content notices;
- deletion/expiry semantics;
- encrypted content;
- peer/operator responsibility;
- cross-border storage;
- abuse reporting.

Email-first deliberately postpones much of this surface.

## Compliance principle

```text
architecture reduces exposure
but
architecture does not replace legal analysis
```
