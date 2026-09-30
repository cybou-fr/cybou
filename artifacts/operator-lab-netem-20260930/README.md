# Initial 30-minute listener-fault run: FAIL

[Immutable failure report](report-20260930T102710-e1d9afc6.json) and SHA-256
are retained. The native generator returned `operations=16 result=PASS`, but
providers did not reach the frozen finalizer height within 120 seconds. This
run is not accepted, despite successful native content/durability checks.

The run applied 100ms listener latency, 10% listener packet loss and 100%
listener loss on three providers. The last action was then named `partition`,
but did not block outgoing peer connections. Later controller versions name
that limited action `listener-partition`; `partition` now blocks all TCP in
the owned namespace. The copied original manifest and reports are unchanged.
Counters confirmed 438 delayed packets, 121 dropped packets for random loss
and 38 dropped packets for the listener partition. All qdiscs were reset.

The lag exposed a routing problem: HELLO heights were connection-time
snapshots, and later connected lagging providers could be preferred over the
configured finalizer. A regression test reproduced partial progress from the
stale peer. Core now prefers configured routes and retains fallback for peers
that fail or stop advancing. All providers eventually caught up after a fresh
connection, but that does not change this failed acceptance result.

Owned processes and the namespace were cleaned up. Cleanup does not delete
the private LAB DBs/vaults; this disposable WSL `/tmp` root can disappear on
WSL shutdown. Public evidence remains here. See the separate fixed-run
artifacts for acceptance of the new implementation.
