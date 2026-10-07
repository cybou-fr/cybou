# 41 — French and European support / funding map

CYBOU should not be designed around grants.

The correct order is:

```text
working implementation
-> verifiable technological depth
-> B2B pilot / PoC
-> operational evidence
-> funding / partnerships for scale
```

## France — strategic fit

CYBOU aligns with French and European strategic priorities when presented as:

```text
cybersecurity infrastructure
digital sovereignty
cryptographic agility and post-quantum transition (hybrid PQ)
resilient interpersonal communications and file distribution
distributed trust systems without foreign SaaS control planes
```

not as a consumer privacy app or speculative cryptocurrency.

## Technological depth vs. Deeptech qualification

CYBOU embodies significant technological depth by engineering its trust architecture
at the protocol level rather than assembling third-party cloud APIs:
- native C++20 Full Node runtime;
- custom P2P protocol over TLS 1.3 with mandatory `X25519MLKEM768` hybrid key exchange;
- account-level cryptographic Identity with domain-separated roles (ML-DSA, Ed25519, X-Wing);
- client-side encrypted content trees with BLAKE3-256 chunk addressing;
- deterministic candidate state execution and independent full-node block validation;
- verified distributed chunk replication and clean-machine restore.

**Important boundary:**
- *Technological depth:* demonstrated in the implementation, test suites, and protocol architecture.
- *Funding category fit:* eligibility for advanced technology and cybersecurity programmes.
- *Formal qualification/status:* CYBOU has **not** received an official French "Deeptech" label, ANSSI visa, or government certification. Any future qualification requires formal application, external evaluation, and empirical evidence.

The project must never use "Deeptech" as an unsupported badge or claim formal status it does not hold.

## French Tech 2030

French Tech 2030 explicitly covers strategic sectors including:
- cybersecurity;
- post-quantum cryptography / transition;
- sovereign digital infrastructure.

The programme targets mature strategic technology demonstrating functional prototypes
in operational environments (TRL 6+).

Implication:
> Architecture alone is not enough; demonstrate working pilots with client-encrypted
> attachments, offline recipient synchronization, and clean-machine recovery under live conditions.

## ANSSI / NCC-FR

The French National Coordination Centre (NCC-FR), operated by ANSSI, helps the French
cybersecurity ecosystem access European funding, particularly:

```text
Digital Europe
Horizon Europe / cybersecurity
European consortium building
```

CYBOU should engage when it has:
- clear technical work packages;
- demonstrable prototype;
- pilot partners (B2B design partners);
- measurable security and resilience objectives.

## Digital Europe

Digital Europe cybersecurity calls cover areas including:
- dual-use cybersecurity technologies;
- strengthening European cyber capacities and infrastructure resilience;
- NCC ecosystem support.

CYBOU's open-source core and sovereign deployment models fit these programmes when
paired with clear European sovereignty and interoperability cases.

## EIC Accelerator

The EIC work programme provides funding for breakthrough and deep technology innovations.
Fit becomes viable only through defensible technological moats beyond "another encrypted email":
- sovereign protocol-level trust core combining Identity, communications, and files;
- independent state verification without third-party authority;
- hybrid classical/post-quantum cryptographic agility;
- verifiable distributed encrypted storage with clean-machine recovery.

## Funding readiness gate

Do not spend founder time on grant applications before all criteria are satisfied:

```text
[ ] working CYBOU Email and Files flows on DEVNET
[ ] verified distributed chunk retrieval and offline recipient recovery
[ ] PoA-finalized RootPublication with transparent single-operator disclosure
[ ] clear intellectual property and open-source architecture definition
[ ] security roadmap and CRA vulnerability disclosure policy
[ ] French legal entity
[ ] committed B2B design partner / pilot interest
[ ] measurable 12–18 month technical work packages
```

## Funding is acceleration, not dependency

The project must retain a self-sustained development path. A grant-dependent
network is not sovereign.
