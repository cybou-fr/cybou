# Delayed scheduler regression: FAIL

[Immutable report](report-20260930T104616-e00827ba.json) records a controller
failure, not a successful 30-minute acceptance. Delayed polling made multiple
scheduled network faults due in one loop. The second fault was refused because
the first still owned the namespace qdisc. The controller reset the fault and
the owned processes/namespace were subsequently cleaned up.

The scheduler now serializes actual outages per namespace, records actual
timing, and fails a run if scheduled faults never execute before its deadline.
A test with deliberately delayed polling covers the ordering of all three
faults and resets. A new fresh LAB root is used for the next run.
