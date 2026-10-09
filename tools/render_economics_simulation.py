#!/usr/bin/env python3
"""Render the BUILD_TESTS-only CYBOU cost model JSON as an explicitly scoped report."""
import argparse
import json
from pathlib import Path


def interval(low, high):
    a, b = f"{low:,}", f"{high:,}"
    return a if low == high else f"{a}–{b}"


def render(data, date, revision):
    rows = data["workloads"]
    if len(rows) != 15 or {row["days"] for row in rows} != {30, 90, 365}:
        raise ValueError("Expected five workloads for three lease durations")
    lines = ["# CYBOU cost simulation", "", "Status: EVIDENCE",
             f"Scope: {date}; source baseline {revision}, offline model using compiled DEVNET parameters.", "",
             f"Rent: {data['rate']} CYBOU/GiB/day/replica; {data['replicas']} replicas; settlement period {data['period_seconds']} seconds.", "",
             "All monetary values below are CYBOU totals for the listed object count. These are arithmetic scenarios, not transactions, payouts, provider assignments or performance measurements.", "",
             "Assumptions: small private metadata fits one ROOT; Mail to another Identity has recipient + self capsules, Files one self capsule. Content imports have one new attachment/content tree per object, without reuse. Metadata-only changes keep referenced content under its original separately funded publication; the row prices only the new metadata publication. No publication consolidation is modeled. Random 160–320 KiB DATA targets give conservative chunk-count bounds; ROOT/INDEX and application ROOT are included. Serialized sizing templates contain placeholders, not valid signatures. Per-object rounding occurs before multiplying by count.", "",
             "The primary debit assumes one initial lease for the full duration. The next column uses the current 30-period initial lease plus one renewal for the remaining duration, including its fee and separately rounded escrow. Renewal is a projection, not an automatic policy.", "",
             "| Scenario | Objects | Periods | Chunks/object | Treasury fee | Escrow | Initial debit | 30 + renewal debit |",
             "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for row in rows:
        for bound in ("min", "max"):
            if row[f"treasury_{bound}"] + row[f"escrow_{bound}"] != row[f"debit_{bound}"]:
                raise ValueError("Inconsistent quote totals")
        lines.append(f"| {row['scenario']} | {row['objects']:,} | {row['days']} | "
                     f"{interval(row['chunks_min'], row['chunks_max'])} | {interval(row['treasury_min'], row['treasury_max'])} | "
                     f"{interval(row['escrow_min'], row['escrow_max'])} | {interval(row['debit_min'], row['debit_max'])} | "
                     f"{interval(row['initial_30_then_renew_debit_min'], row['initial_30_then_renew_debit_max'])} |")
    lines += ["", "Full-service accrual model and refund", "",
              "The following floor-with-carried-remainder model assumes sufficient verification for the entire interval. It is not the current canonical per-period payout schedule. Actual payouts/refunds require accepted evidence and PoA-finalized settlements; they are not known from object sizes. Refund values are endpoints for the two chunk assumptions, not bounds for all intermediate counts.", "",
              "| Scenario | Periods | Accrued model | Refund at min chunks | Refund at max chunks |",
              "|---|---:|---:|---:|---:|"]
    for row in rows:
        lines.append(f"| {row['scenario']} | {row['days']} | {interval(row['full_service_floor_min'], row['full_service_floor_max'])} | "
                     f"{row['refund_model_at_min_chunks']:,} | {row['refund_model_at_max_chunks']:,} |")
    lines += ["", "Provider sensitivity", "",
              "Equal-share floor per provider for the combined 30-period full-service model; allocation remainders are excluded. This assumes equal demand distribution, not independent hosts or the actual selector. Onboarding-origin earnings remain System Balance, not transferable Balance.", "",
              "| Providers | Model share per provider |", "|---:|---:|"]
    for row in data["provider_models"]:
        lines.append(f"| {row['providers']} | {interval(row['equal_share_floor_min'], row['equal_share_floor_max'])} |")
    lines += ["", f"{data['onboarding_count']:,} new ordinary Identities transfer {data['onboarding_treasury_debit']:,} CYBOU from Treasury to System Balance; no minting. This measures Treasury exposure, not the full cost of a Sybil attack.", "",
              f"Rounding finding: one billing unit has {data['tiny_30_day_escrow']} CYBOU escrow for 30 periods and a {data['tiny_daily_cap']} CYBOU current one-period cap. The first period can therefore consume the whole escrow if eligible settlement evidence is accepted. Floor accrual for the complete 30-period interval is zero with a retained fractional remainder. A corrected payout schedule requires an explicit consensus decision; this report changes no tariff or protocol.", "",
              "Reproduce from a BUILD_TESTS build:", "", "```powershell",
              "cmake --build build_cybou_qt_mingw --target cybou-economics-simulation",
              ".\\build_cybou_qt_mingw\\bin\\cybou-economics-simulation.exe | Out-File -Encoding utf8 artifacts/economics-simulation.json",
              "python tools/render_economics_simulation.py artifacts/economics-simulation.json --output artifacts/economics-simulation.md --date 2026-10-09 --revision 7afe1ec9",
              "```", ""]
    return "\n".join(lines)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--date", required=True)
    parser.add_argument("--revision", required=True)
    args = parser.parse_args()
    data = json.loads(args.input.read_text(encoding="utf-8-sig"))
    args.output.write_text(render(data, args.date, args.revision), encoding="utf-8")
