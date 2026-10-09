# Built-in Network overview

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: active product/implementation scope, 2026-10-09, DEC-289/DEC-290.

## Exactly three primary figures

| Figure | Existing source | Honest precision |
|---|---|---|
| Operations/min | Last positive FinalizationMeter observation from a completed 60-second window; initially 5 | One retained figure, saved locally per exact NetworkID. Initial 5 is a presentation default, not measured traffic, throttling or a maximum. Excludes history imports, Battle Test references and artificial/theoretical rates. |
| Network storage capacity | Sum of responding nodes' existing FinalizedChunkStore CapacityBytes provider budgets | Approximate reported usable provider capacity, not a census or guaranteed free disk. Local reserve excluded. |
| Data hosted by the network | Sum of responding nodes' existing FinalizedChunkStore UsedBytes admission counters | Approximate accounted admitted encrypted bytes. Copies on distinct stores count; no unique-file or independently audited holding claim. |

The operator wants useful approximate figures, not proof of every byte. Do not
gate these readings on a new audit/notarization/accounting system, a network
census, independent failure domains or a new capacity benchmark. Achieved maxima
come only from real measurements during ordinary operation, never imported test
results, generated load or a theoretical ceiling.

## Small direct query, no telemetry platform

Every Full Node can answer a direct ordinary TLS storage query with two existing
u64 counters. STORAGE_USAGE_REQUEST (27) has no payload; STORAGE_USAGE (28) is
exactly provider_capacity:u64 LE and admitted_bytes:u64 LE. No new signature,
audit or proof of bytes is created. The existing on-demand storage identity proof
is reused solely to deduplicate the storage relationship, never as authority.

The existing independent storage probe queries known/configured endpoints one
at a time, at most once per endpoint per 30 seconds. It uses ordinary admission,
TLS pinning, ingress/transfer limits and a two-second usage reply deadline.
Only fresh direct samples up to 90 seconds old enter a sum, once per StorageId;
never recursively sum another node's aggregate. Include this Full Node once.
Overflow yields unavailable values; missing replies do not mean zero.
The small in-memory sample fits the existing bounded storage-probe map; no new
DB, worker, CPU/RAM report, cohort history, provider registry or consensus state.

## Presentation

Preserve the map's fitted size and center. Exactly three figures appear
horizontally in a translucent overlay over its top/header. No lateral panel,
no layout space reserved above the map and no historical benchmark card in
Network or Advanced. The summary remains visible when Advanced opens; the drawer
starts below it. Smaller numeric text and reduced map margins preserve a large map.
Capacity/hosted values carry ≈, sampled-node/reply coverage
and observation time. These are indicative totals for reached nodes, not a
guarantee that every participant has answered.

Operations/min is one displayed value. At first launch it is 5. Each new positive
completed-minute observation replaces it, including decreases (12 → 8), and is
saved in existing local settings under `network/<canonical NetworkID hex>/operationsPerMinute`.
Zero, missing/incomplete measurements, offline state and synchronization do not
overwrite it; the next launch in the same network restores the saved value.
A status-only change cannot reuse a rejected synchronization sample as a new
measurement. Saving continues while the Network page is hidden. The tooltip
explains the last positive observation and initial default; no Base operations/min
card, separate base metric or current-rate line. Local diagnostic charts retain
their real zero observations. No new DB, P2P message, telemetry service or
consensus field is needed.

Each figure has a small observed-maximum value. The runtime retains the greatest
completed-minute operation observation and greatest sampled capacity/admitted
byte sum since this node started. These are maxima actually observed by this
runtime, not guaranteed global all-time records. They reset with runtime restart
or explicit meter reset; no persistent telemetry ledger or synthetic load.
The persisted display value and initial 5 never seed or update these maxima.

Local CPU/RAM, disk, PUT/GET, uptime and candidate pool remain Advanced/Console
details; they are not primary network figures. A Home node summary is separate
future work. Keep only the existing two local charts (finalized operations and
completed PUT/GET), with no remote resource chart.

## Delivery and acceptance

The direct counter query, deduplication/sum and overlay are source-implemented.
Test the real TLS exchange, malformed response bounds, duplicate/expired samples,
zero/partial states and native EN/FR layout. Update the ordinary peer executables
before accepting live coverage; synthetic totals are layout evidence only.
No network/genesis/key/history reset, new PoA route or alternate node type.
Live restore/rotation/payment/durability Beta acceptance remains separate.

Operations/min acceptance: 5 → 12 → 8 → idle (8) → restart (8) → new
measurement 17. Verify settings persistence, NetworkID isolation, skipped
partial/synchronizing/absent samples, hidden-page saving and a real measured
maximum below 5 without substitution by the initial default.
