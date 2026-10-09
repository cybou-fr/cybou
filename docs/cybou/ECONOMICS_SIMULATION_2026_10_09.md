# CYBOU cost simulation

Status: EVIDENCE
Scope: 2026-10-09; source baseline 7afe1ec9 (simulation sources in this package), offline model using compiled DEVNET parameters.

Rent: 5 CYBOU/GiB/day/replica; 2 replicas; settlement period 86400 seconds.

All monetary values below are CYBOU totals for the listed object count. These are arithmetic scenarios, not transactions, payouts, provider assignments or performance measurements.

Assumptions: small private metadata fits one ROOT; Mail to another Identity has recipient + self capsules, Files one self capsule. Content imports have one new attachment/content tree per object, without reuse. Metadata-only changes keep referenced content under its original separately funded publication; the row prices only the new metadata publication. No publication consolidation is modeled. Random 160–320 KiB DATA targets give conservative chunk-count bounds; ROOT/INDEX and application ROOT are included. Serialized sizing templates contain placeholders, not valid signatures. Per-object rounding occurs before multiplying by count.

The primary debit assumes one initial lease for the full duration. The next column uses the current 30-period initial lease plus one renewal for the remaining duration, including its fee and separately rounded escrow. Renewal is a projection, not an automatic policy.

| Scenario | Objects | Periods | Chunks/object | Treasury fee | Escrow | Initial debit | 30 + renewal debit |
|---|---:|---:|---:|---:|---:|---:|---:|
| short_mail | 10,000 | 30 | 1 | 240,000 | 10,000 | 250,000 | 250,000 |
| mail_attachment_256_kib | 1,000 | 30 | 3–4 | 32,000–36,000 | 1,000 | 33,000–37,000 | 33,000–37,000 |
| document_1_mib | 100 | 30 | 6–9 | 4,000–5,200 | 100–200 | 4,100–5,400 | 4,100–5,400 |
| file_1_gib | 10 | 30 | 3,305–6,608 | 132,360–264,480 | 4,850–9,680 | 137,210–274,160 | 137,210–274,160 |
| metadata_changes | 1,000 | 30 | 1 | 20,000 | 1,000 | 21,000 | 21,000 |
| short_mail | 10,000 | 90 | 1 | 240,000 | 10,000 | 250,000 | 270,000 |
| mail_attachment_256_kib | 1,000 | 90 | 3–4 | 32,000–36,000 | 2,000 | 34,000–38,000 | 35,000–40,000 |
| document_1_mib | 100 | 90 | 6–9 | 4,000–5,200 | 300–400 | 4,300–5,600 | 4,400–5,800 |
| file_1_gib | 10 | 90 | 3,305–6,608 | 132,360–264,480 | 14,530–29,040 | 146,890–293,520 | 146,910–293,530 |
| metadata_changes | 1,000 | 90 | 1 | 20,000 | 1,000 | 21,000 | 23,000 |
| short_mail | 10,000 | 365 | 1 | 240,000 | 20,000 | 260,000 | 280,000 |
| mail_attachment_256_kib | 1,000 | 365 | 3–4 | 32,000–36,000 | 6,000–8,000 | 38,000–44,000 | 39,000–45,000 |
| document_1_mib | 100 | 365 | 6–9 | 4,000–5,200 | 1,100–1,700 | 5,100–6,900 | 5,200–7,000 |
| file_1_gib | 10 | 365 | 3,305–6,608 | 132,360–264,480 | 58,910–117,770 | 191,270–382,250 | 191,290–382,260 |
| metadata_changes | 1,000 | 365 | 1 | 20,000 | 2,000 | 22,000 | 24,000 |

Full-service accrual model and refund

The following floor-with-carried-remainder model assumes sufficient verification for the entire interval. It is not the current canonical per-period payout schedule. Actual payouts/refunds require accepted evidence and PoA-finalized settlements; they are not known from object sizes. Refund values are endpoints for the two chunk assumptions, not bounds for all intermediate counts.

| Scenario | Periods | Accrued model | Refund at min chunks | Refund at max chunks |
|---|---:|---:|---:|---:|
| short_mail | 30 | 0 | 10,000 | 10,000 |
| mail_attachment_256_kib | 30 | 0 | 1,000 | 1,000 |
| document_1_mib | 30 | 0–100 | 100 | 100 |
| file_1_gib | 30 | 4,840–9,670 | 10 | 10 |
| metadata_changes | 30 | 0 | 1,000 | 1,000 |
| short_mail | 90 | 0 | 10,000 | 10,000 |
| mail_attachment_256_kib | 90 | 1,000 | 1,000 | 1,000 |
| document_1_mib | 90 | 200–300 | 100 | 100 |
| file_1_gib | 90 | 14,520–29,030 | 10 | 10 |
| metadata_changes | 90 | 0 | 1,000 | 1,000 |
| short_mail | 365 | 10,000 | 10,000 | 10,000 |
| mail_attachment_256_kib | 365 | 5,000–7,000 | 1,000 | 1,000 |
| document_1_mib | 365 | 1,000–1,600 | 100 | 100 |
| file_1_gib | 365 | 58,900–117,760 | 10 | 10 |
| metadata_changes | 365 | 1,000 | 1,000 | 1,000 |

Provider sensitivity

Equal-share floor per provider for the combined 30-period full-service model; allocation remainders are excluded. This assumes equal demand distribution, not independent hosts or the actual selector. Onboarding-origin earnings remain System Balance, not transferable Balance.

| Providers | Model share per provider |
|---:|---:|
| 10 | 484–977 |
| 100 | 48–97 |
| 1000 | 4–9 |

10,000 new ordinary Identities transfer 200,000,000 CYBOU from Treasury to System Balance; no minting. This measures Treasury exposure, not the full cost of a Sybil attack.

Rounding finding: one billing unit has 1 CYBOU escrow for 30 periods and a 1 CYBOU current one-period cap. The first period can therefore consume the whole escrow if eligible settlement evidence is accepted. Floor accrual for the complete 30-period interval is zero with a retained fractional remainder. A corrected payout schedule requires an explicit consensus decision; this report changes no tariff or protocol.

Reproduce from a BUILD_TESTS build:

```powershell
cmake --build build_cybou_qt_mingw --target cybou-economics-simulation
.\build_cybou_qt_mingw\bin\cybou-economics-simulation.exe | Out-File -Encoding utf8 artifacts/economics-simulation.json
python tools/render_economics_simulation.py artifacts/economics-simulation.json --output artifacts/economics-simulation.md --date 2026-10-09 --revision 7afe1ec9
```
