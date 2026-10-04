# Security, privacy and resilience governance

Reviewed: 2026-10-04. This is CYBOU's governing security and privacy baseline,
above internal architecture, product goals and implementation convenience.
Cryptographic standards are a supporting technical layer, not the whole baseline.

## Priority and applicability

1. Applicable French/EU law and regulatory obligations, including RGPD/GDPR and
   NIS2 as applicable through national law, determine mandatory duties.
2. CYBOU adopts ANSSI risk-management/hygiene guidance and ISO/IEC 27001/27002
   as engineering and governance references. Record scope and selected controls;
   recommendations do not become statutory duties merely by being cited.
3. Evaluate every relevant design against confidentiality, integrity and
   availability (CIA), together with privacy rights, accountability and resilience.
4. Select technical standards and implementation mechanisms to meet these
   objectives. Internal frozen decisions must be revised when they conflict
   with applicable obligations or accepted security requirements.

Document the conflict, control evidence and compatibility plan. This priority
does not authorize replacing immutable genesis, creating keys, changing hash
domains, or deploying a network cutover. Security requirements guide that work;
they do not themselves grant operational authorization.

## Governing references

| Reference | Role and verified source | CYBOU requirement / open evidence |
|---|---|---|
| RGPD/GDPR, Regulation (EU) 2016/679 | [Official regulation](https://eur-lex.europa.eu/eli/reg/2016/679/oj/eng); arts. 5, 6, 17, 24, 25, 30, 32–35 as applicable | Determine processing purposes, lawful bases and controller/processor roles; minimize public metadata and logs; define retention and rights handling; assess DPIA need, security measures and breach procedures. Encryption and France-only IP admission do not settle these duties |
| CNIL | [Developer security guidance](https://www.cnil.fr/fr/securite-encadrer-les-developpements-informatiques) and [rights/erasure](https://www.cnil.fr/fr/reglement-europeen-protection-donnees/chapitre3) | Privacy requirements belong in design and acceptance tests; deletion must explain historical chain data, capsules, bridges, backups and recipient copies. Do not equate revocation with satisfying every erasure request |
| ANSSI | [EBIOS Risk Manager](https://cyber.gouv.fr/securisation/analyse-des-risques/methode-ebios-rm/) and [hygiene guide](https://messervices.cyber.gouv.fr/guides/guide-dhygiene-informatique) | Scope assets, feared events, adversaries and dependencies; prioritize controls for privileged access, key custody, updates, logging, backups and incident response. Record residual risk and its owner; no ANSSI certification is claimed |
| NIS2, Directive (EU) 2022/2555 | [Official directive](https://eur-lex.europa.eu/eli/dir/2022/2555/oj); [ANSSI applicability information](https://cyber.gouv.fr/reglementation/cybersecurite-systemes-dinformation/directives-nis-nis2-et-dispositif-saiv/directive-nis-2/) | Establish entity/service/size/jurisdiction applicability and current French transposition requirements. Prepare governance, risk controls, supply-chain security, continuity and incident reporting; do not assume every user or full node is a regulated entity |
| ANSSI ReCyF | The ANSSI page above describes the 2026-03-17 publication as a working document, non-mandatory by default at the review date | Use as a preparation/control mapping reference, not evidence of enacted French requirements or CYBOU conformity. Recheck legal/reference status before a release or compliance declaration |
| ISO/IEC 27001:2022 | [ISO publication](https://www.iso.org/standard/27001), with publisher-listed amendment | Define ISMS scope, risk assessment/treatment, assigned responsibilities, evidence review and continuous improvement. A software test suite is not an ISMS or a certification audit |
| ISO/IEC 27002:2022 | [ISO publication](https://www.iso.org/standard/75652.html), information security control guidance | Select and justify controls against risks; maintain a Statement of Applicability for the intended ISMS scope. Full clause-level mapping requires the authorized standard text; this review uses public publisher summaries |
| CIA triad | [ISO explanation](https://www.iso.org/standard/27001) | Treat confidentiality, integrity and availability as concurrent objectives. CIA is an analysis model, not legislation or a certification scheme |

The broader release assessment also includes CRA and other applicable duties
in [`40_REGULATORY_READINESS_FR_EU.md`](40_REGULATORY_READINESS_FR_EU.md).
The cryptographic/transport register is [`SECURITY_STANDARDS.md`](SECURITY_STANDARDS.md).

## CIA and privacy acceptance matrix

| Objective | Existing engineering evidence | Required next evidence / limits |
|---|---|---|
| Confidentiality | Encrypted application content and per-Identity Application DB; separate key roles | Inventory plaintext/key copies, metadata and logs; test access boundaries and backup exposure. Capsule encryption does not hide every network/chain identifier |
| Integrity | Local execution, finalized state/PoA verification, Merkle authorization and exact-chunk hash checks | Review signature/KEM transcripts and official vectors; verify crash boundaries and update provenance. Authentic bytes alone do not establish lawful processing |
| Availability | Surviving-replica repair; empty-node recovery through current mnemonic and historical bridge | Measure RPO/RTO, restore time and independent failure domains; exercise finalizer/discovery/Geo-data outages and recovery of signing history. Component tests do not establish a service SLA |
| Privacy and deletion | Finalized revocation, shared-chunk retention, durable purge retry; restored catalog excludes deleted file | Classify retained public data and handling of rights requests, backups and lawful retention. Historical keys/capsules/bridges prevent the current crypto-erasure claim |
| Governance and response | Documented engineering boundaries and regression evidence | Assign accountable owners; complete risk register, control selection, incident/breach playbooks and supply-chain/update evidence. Ownership and compliance applicability remain open |

The project deliberately has a single authorized PoA finalizer. Its outage risk
must be evaluated under availability, with a documented continuity procedure
that preserves one signer and durable signing safety. Replica repair cannot
remove that finalization dependency, and standards do not mandate inventing a
new consensus authority.

## Required decision record

Each significant design/change records: affected assets and processing;
applicable legal/reference requirement; C/I/A and privacy impact; control and
implementation evidence; remaining risk; accountable reviewer; and release
criterion. Use explicit statuses: evidenced within stated scope, partial, open,
or not applicable with justification. No compliance/certification claim follows
from a reference link, architectural intention or successful unit test.

First priorities: processing/metadata inventory and roles; risk scenarios and
control selection; recovery/retention objectives; incident and breach response;
then technical conformance gaps. No owners or compliance status are invented by
this document.
