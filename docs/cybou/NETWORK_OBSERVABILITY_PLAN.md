# Built-in Network monitoring — Beta scope

Status: active Level 4 delivery boundary and Level 5 metric contract, 2026-10-09.
DEC-289 freezes Beta monitoring to passive local measurements and evidence
naturally produced by the existing chain, P2P and storage protocols. No separate
remote telemetry protocol, report cache, polling worker, address-group aggregation
or cohort history is part of the active runtime. Git retains the former design.

## Frozen metric set

| Metric | Source and scope |
|---|---|
| Status / safety halt / sync | This Full Node's runtime and verified state; peer sync is only a liveness hint |
| Finalized height | Locally independently verified PoA-finalized chain |
| Connected peers | Current admitted local mesh sessions; not a network census |
| Finalized operations/min | One verified stream, completed 1/5/15-minute local arrival windows; historical imports separate |
| Local storage | Explicit V, accounted encrypted copies, admitted provider bytes/budget and available filesystem bytes |
| PUT/GET payload | Completed encrypted transfers, receive/send totals and 60-complete-second rates; repeats included |
| Content protection | Existing Identity-scoped Mail/Files durability model and storage-service evidence |
| Candidate pool / uptime | Actual volatile local pool and runtime lifetime, in Technical/Console |
| CPU / RAM / frame traffic | Local Technical/Console diagnostics; no remote values or network load score |

A complete quiet window may be zero. Startup, missing/failed samples and partial
windows stay Unknown. Measurements reset with the runtime, carry its network
binding/sample time, and retain no peer/user/content identifiers in metric history.
The existing diagnostics sampler continues independently of Identity unlock and
Network visibility. Reading the UI triggers no transfer, proof, audit or benchmark.

PUT receive counts successful admission; PUT send requires a matching verified
provider receipt. GET receive requires complete ChunkID verification; GET send
means a complete local write, without proof of remote receipt. Completion-time
windows include recovery/repair/full GET checks and repeat transfers. Endpoint
totals can differ. Frame traffic includes service/retries and excludes TLS/TCP
headers; it is separate from completed encrypted payload.

Storage V and physical use are not unique logical content, verified durability,
service-eligible remote capacity or consensus rights. Available shared disk space
is not a promised allocation. Peer reachability/height are not PoA authority,
independent failure domains or proof of global freshness. Storage evidence remains
service-owned; GUI pages do not enumerate provider databases or the common store.

## Presentation

Network / Réseau presents connection/chain progress, storage, transfers and
existing content protection. Local CPU/RAM, uptime, candidate pool, frame traffic,
peer table, StorageIds, state root and NetworkBinding belong in Advanced/Technical
or Console. The illustrative France map is an observed-connection view only.

Keep at most two local bounded charts: finalized operations and completed PUT/GET
payload receive/send. Both use completed five-second intervals, at most 180 points
(15 minutes) in volatile RAM. No network CPU, frame-traffic or capacity chart.
Console `health`, `metrics` and `capacity` remain read-only and match local sources.

## Addition gate

The Beta metric set is frozen. Do not automatically continue the former queue of
storage I/O, queue age, error counters, canonical register totals, resource history,
remote consolidation or theoretical capacity estimates. A proposed addition must
answer a concrete operator/product question (for example, why this publication
cannot obtain a replica), identify an existing trustworthy source, define its
scope/cost/privacy and receive an explicit scope decision before implementation.

Future storage views may summarize real receipt/audit/repair evidence, but proven
StorageIds alone do not establish hosts or operators; a percentage needs a defined
sample/window and denominator. No global capacity or throughput ceiling follows
from local resource headroom. Do not invent provider, replica, audited-service or
protected-byte totals absent a service-owned evidence definition.

Beta acceptance remains live restore/rotation, uncertain payments, retrieval,
repair, independent remote failure domains and native desktop usability. Optional
historical capacity benchmarks validate a separate ceiling and never replace
monitoring. No network/genesis/key/history change or deployment is required by
this source simplification.
