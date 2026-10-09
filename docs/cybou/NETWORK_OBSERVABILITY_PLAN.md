# Built-in Network monitoring — Beta scope

Status: active Level 4 delivery boundary and Level 5 metric contract, 2026-10-09.
DEC-289 freezes Beta monitoring to passive local measurements and evidence
naturally produced by the existing chain, P2P and storage protocols. No separate
remote telemetry protocol, report cache, polling worker, address-group aggregation
or cohort history is part of the active runtime. Git retains the former design.

## Frozen metric set

### Network product target — corrected scope (2026-10-09)

The operator explicitly requested three network-wide figures. The primary
Network page must answer these questions, preserving the full-map composition:

| Primary figure | Meaning | Current evidence and delivery gap |
|---|---|---|
| Current operations/min | Operations finalized in the single verified network stream during a defined observation window | Existing finalization meter observes verified block arrivals; complete windows, sync/import state and sample time must remain explicit. It is not this node's produced-operation rate, sum of peer rates or a global freshness guarantee. |
| Maximum network throughput | Measured sustainable finalized operations/min under a stated workload and network configuration | No established network-wide maximum exists. The historical Files simulation measures its workload/cohort, not a ceiling. Define an accepted capacity procedure, saturation/error/latency criteria and configuration before publishing a number. Never derive it from local CPU, storage V, current rate or protocol block bounds. |
| Data actually hosted by the network | Actual encrypted payload currently held by the network, with a defined copy accounting basis | No complete aggregate exists. Canonical publications/leases record authorization and billing units, not exact byte lengths or current holdings. Define storage-service evidence, replica accounting, deduplication, freshness, coverage and deletion handling before collecting/publishing the total. Local ChunkStore use, owned file sizes, paid units and chunk count × 512 KiB are not this figure. |

These three figures are the accepted product scope, not implemented telemetry.
They replace the mistaken interpretation that local storage and PUT/GET cards
constitute the requested network overview. Do not introduce a fourth primary
storage-capacity figure or substitute a local reading for a missing network value.
Do not restore the former broad peer resource-report protocol as a side effect.
The source/evidence work below must precede numeric claims and GUI delivery.

Local node health/storage/transfer diagnostics belong in a compact Home summary
and detailed Advanced/Console surfaces. That Home summary is target work;
restoring the large Network map did not implement it. CPU/RAM remain technical.

### Next delivery work

1. Specify a small service-owned network summary for the three figures above.
   Record units, observation window, network binding, as-of time, evidence source,
   known coverage, Unknown/partial/stale states and collector cost. Separate
   independently verified chain facts, scoped measured capacity and storage
   evidence; no observation grants consensus authority.
2. For hosted data, decide and expose unique encrypted payload versus physical
   replica bytes without summing duplicate peer reports. Define the observation
   population and privacy boundary; StorageId alone is no host/operator census.
   Existing receipts/audits are relationship-scoped evidence, not a network total.
3. Establish the throughput-capacity acceptance procedure on the actual supported
   configuration. A workload's highest achieved rate is scoped evidence, not
   proof of an absolute maximum. Retain the run/configuration/date with the value.
4. Wire the resulting summary into one compact surface within the existing map
   composition. No strip of three diagnostic cards above the map. Keep local
   numbers in Home/Advanced and retain exactly the existing two chart types.
5. Verify source correctness, missing/partial/stale states, replica deduplication
   and native EN/FR layout before calling the network summary complete.

### Existing implemented local diagnostics

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

The primary Network / Réseau target is the three network figures specified above;
the restored runtime still shows the map, local sessions/protection and historical
reference, with diagnostic readings in Advanced. Local CPU/RAM, uptime, candidate pool, frame traffic,
peer table, StorageIds, state root and NetworkBinding belong in Advanced/Technical
or Console. The illustrative France map is an observed-connection view only.

Keep at most two local bounded charts: finalized operations and completed PUT/GET
payload receive/send. Both use completed five-second intervals, at most 180 points
(15 minutes) in volatile RAM. No network CPU, frame-traffic or capacity chart.
Console `health`, `metrics` and `capacity` remain read-only and match local sources.

## Addition gate

The existing local Beta metric set is frozen. The three-figure network product
request above is an explicit scope decision; it does not authorize invented
numeric estimates or automatically revive the former remote-report architecture.
Do not automatically continue the former queue of
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
