# 30-minute isolated network acceptance: PASS

See [immutable report](report-20260930T111948-3a26165c.json) and its SHA-256
companion. A fresh WSL namespace ran one finalizer, three storage providers,
one observer and two native synthetic Identity clients for 1800 seconds.
The bounded Files workload produced 16 RootPublications plus two Identity
creations. The native client returned `operations=16 result=PASS`, checking
synthetic file bytes and a target of two remote full replicas.

The controller verified 3600 canonical heights and 16 finalized Identity
nonces. All five daemon nodes reached the frozen finalizer height 3597:
finalizer/provider-a 3597, provider-b/provider-c 3598, observer 3599. No
canonical divergence, safety halt, sequence gap or duplicate nonce was seen.

Three scheduled faults were applied and reset, with actual timing in
`chaos.jsonl`:

- provider-a listener latency 100ms: 491 shaped packets;
- provider-b listener loss 10%: 125 dropped packets;
- full namespace TCP partition: 66 dropped packets.

Both host and LAB loopback returned to their original noqueue qdisc. The owned
processes and namespace were cleaned up after the run. Cleanup retains private
LAB files, but this disposable WSL `/tmp` root can disappear on shutdown.

This run validates the configured-route priority fix that followed the first
long FAIL. A subsequent core change also continues partially filled batches
through other peers; that final build has its own unit and short fault-run
acceptance. The long run used these binary SHA-256 values:

- cybou-node: `3cbe21f5540552745b6ab724a2a27e708e6c4e45810c0d57f806646d23cd058f`
- cybou-loadgen: `b97350ba5e15aa182895cc1b923650323bbc78e70de8f30d537250cacc651db5`

Submit-to-finality percentiles were 257/861/952ms (p50/p95/p99). Protection
and repair timing samples were absent; no SLA follows from these observations.
Resource summaries contain sampled bounds, not continuous peaks.

Two-hour/overnight runs, per-node routed partitions, cross-host NAT provider
acceptance, storage pressure and scheduled workload spikes remain unverified.
This single-host namespace topology does not certify independent physical
failure domains. The earlier failed runs and their hashes remain separate.
