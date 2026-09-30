# Final build fault acceptance: PASS

[Immutable report](report-20260930T112526-68d9326b.json) and its SHA-256
companion cover the final core build, including partial-batch failover.
The 120-second fresh WSL LAB used one finalizer, three providers, one observer
and two native synthetic Identity clients. It finalized 16 Files publications
and two Identity creations; the native client verified exact synthetic bytes
and the target of two remote replicas. All five daemon nodes reached the
frozen height 249. Across 251 verified heights there was no canonical
divergence, safety halt, repeated nonce or sequence gap.

The run applied listener latency, listener packet loss and a full namespace
TCP partition, followed by resets and mandatory convergence. Owned processes
and the namespace were removed; host loopback was unchanged. Private LAB
files were not copied here. The disposable `/tmp` root can disappear on WSL
shutdown. Report hashes were independently verified.

The preceding [30-minute acceptance](../operator-lab-netem-fixed2-20260930/README.md)
passed on the configured-route priority build. This short acceptance covers
the later change that queries other peers after partial progress, sharing one
bounded batch budget. The regression test failed on both old behaviors and
passes for stale HELLO priority, stopped-primary fallback and partially
advancing primary fallback. Final relevant core suites passed 43 test cases;
Windows CLI acceptance and all 44 native Qt tests passed. The earlier full
Linux run passed all 46 CTest suites.

Final binary SHA-256:

- cybou-node: `e8bb57971abc20dc75f178c5b05279cb1daba2c4655b7dbc895059b03fd1a897`
- cybou-loadgen: `b796ea6e7139de86d0a8438f9099febf2b76e67302a3eb3c913d702997cafbc5`

DEV was rebuilt and all three services restarted without resetting state or
changing the PoA key. Ports 29461/29471/29481 listened; all three finalized
heights advanced together from 81668 to 81821. Rollback executables are retained
as `cybou-node.rollback-before-sync-routing-20260930` and
`cybou-node.rollback-before-sync-partial-20260930` in DEV `build/bin`.

Two-hour/overnight runs, per-node routed partition, cross-host NAT provider
acceptance, storage pressure and workload spikes remain outside this evidence.
