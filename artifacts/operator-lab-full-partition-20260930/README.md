# Full namespace TCP partition acceptance

The 90-second real-node run passed. See
[immutable report](report-20260930T101958-1c471196.json) and its SHA-256 companion.
The topology was one WSL Linux namespace with a finalizer, three providers,
an observer and two native synthetic Identity clients. Two Files publications
used the Beta target of two remote replicas; the native client returned PASS.

The partition blocked all TCP on the owned namespace loopback, including
outgoing and newly created connections. It was applied at 30 seconds and
reset roughly 13.5 seconds later (configured duration 10 seconds, with polling
and transport overhead). Final netem counters recorded 62 drops. During the
partition, after a 500ms boundary margin, the finalizer emitted 26 blocks and
no other node emitted a finalized-block event. After recovery all five daemon
nodes independently verified the frozen finalizer height 194. The controller
verified canonical agreement across 195 heights and four operations, including
two Identity creations. Host loopback retained its original noqueue qdisc.

The owned processes and namespace were stopped and removed after acceptance;
cleanup does not delete the LAB DBs and private vaults outside this repository.
The disposable WSL `/tmp` root can disappear on WSL shutdown. This is a short
network-wide partition acceptance, not per-node
routed partition, cross-host NAT or long-run certification. Protection and
repair latency samples are absent in these binaries; null values establish no
latency SLA.

Binary SHA-256:

- cybou-node: `106e6e1d67bec690ac205a0a7acdeccc603813e1d87b38ba18e042bb7f876ae4`
- cybou-loadgen: `a0c80c8757aed9cebdb34e6ca02108d7e9d0a539062e56103e4013d06df69992`
