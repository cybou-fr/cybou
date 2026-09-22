# 43 — Strategic source register

Official/public references used in the v0.6 France/EU strategy pass.

This file is evidence for planning, not a substitute for legal advice.

## France

### French Tech 2030

- La Mission French Tech — French Tech 2030
- https://lafrenchtech.gouv.fr/fr/programme/french-tech-2030/

Relevant themes: cybersecurity, digital infrastructure, strategic technology, TRL expectations.

### ANSSI — NCC-FR

- https://cyber.gouv.fr/offre-de-service/ncc-fr/
- https://cyber.gouv.fr/offre-de-service/ncc-fr/faq-financements-europeens/

Relevant themes: French cybersecurity ecosystem, Digital Europe/Horizon Europe, consortium support.

### ANSSI — cryptography controls

- https://cyber.gouv.fr/reglementation/reglementation-identite-confiance-numerique/controles-reglementaires-cryptographie/controle-moyen-de-cryptologie/
- https://cyber.gouv.fr/reglementation/reglementation-identite-confiance-numerique/controles-reglementaires-cryptographie/controle-moyen-de-cryptologie/controle-rglementaire-cryptographie-demarches/
- https://cyber.gouv.fr/reglementation/reglementation-identite-confiance-numerique/controles-reglementaires-cryptographie/controle-export/

Relevant themes: use, provision/import/intra-EU transfer/export, dual-use controls.

### AMF — MiCA transition

- AMF, 6 July 2026 — end of Pacte/MiCA transitional period.
- https://www.amf-france.org/fr/actualites-publications/communiques/communiques-de-lamf/crypto-actifs-la-fin-de-la-periode-de-transition-entre-la-loi-pacte-et-le-reglement-europeen-mica

### CNIL — client-side encryption

- https://www.cnil.fr/fr/les-pratiques-de-chiffrement-dans-linformatique-en-nuage-cloud-public

Relevant theme: client-controlled keys can prevent storage provider access to plaintext.

## European Union

### Digital Europe Programme

- https://digital-strategy.ec.europa.eu/en/activities/digital-programme
- https://digital-strategy.ec.europa.eu/en/activities/get-funding-digital

Relevant themes: cybersecurity capacities, EU regulatory readiness, dual-use cyber technologies.

### Cyber Resilience Act

- https://digital-strategy.ec.europa.eu/en/policies/cra-reporting
- https://digital-strategy.ec.europa.eu/en/policies/cra-summary
- https://digital-strategy.ec.europa.eu/en/factpages/cyber-resilience-act-implementation

Relevant dates:
- 11 September 2026: CRA reporting obligations start applying;
- 11 December 2027: full CRA application.

### Digital Services Act

- Regulation (EU) 2022/2065
- https://eur-lex.europa.eu/eli/reg/2022/2065

Relevant distinction: private interpersonal communications between a finite number of recipients are not an online platform merely on that basis; public/open distribution changes the analysis.

### EIC 2026

- https://eic.ec.europa.eu/eic-funding-opportunities/eic-2026-work-programme_en

Relevant theme: strategic/deep-tech innovation and scale-up funding.

## Competitive references

### Olvid

- https://olvid.io/faq/manuals/
- https://olvid.io/press/fr/

Published security material includes historical ANSSI CSPN certification reports.

### Tchap

For competitive research, use official French government / Interoperable Europe material where available. CYBOU should not claim or imply Tchap replacement without a specific customer requirement.

## Review cadence

Regulatory/funding information changes.

Review this register:

```text
before grant application
before public beta
before token economic launch
before non-EU commercial distribution
at least every 6 months during pre-production
```


## Email cryptography references

### HPKE
- RFC 9180 — Hybrid Public Key Encryption
- https://www.rfc-editor.org/rfc/rfc9180.html

### ML-KEM
- NIST FIPS 203 — Module-Lattice-Based Key-Encapsulation Mechanism Standard
- https://csrc.nist.gov/pubs/fips/203/final

### PQ/T HPKE
- IETF HPKE WG, `draft-ietf-hpke-pq`
- https://datatracker.ietf.org/doc/draft-ietf-hpke-pq/

As of September 2026 this remains work in progress. Current draft text includes ML-KEM and hybrid KEMs including MLKEM768-X25519.
