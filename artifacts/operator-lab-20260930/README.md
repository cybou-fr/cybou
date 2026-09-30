# Operator CLI / Network Lab: implementation and acceptance

Implementation includes strict named CLI commands, independent observer mode,
read-only core diagnostics, typed public JSONL, desktop Network Monitor,
production-service native loadgen and a TOML controller using local/Windows,
WSL and SSH workers. No REST, JSON-RPC, new wire entity or Validation runtime
was introduced. DEV keeps the existing network, state and PoA key.

## Evidence

- Linux: all 46 CTest suites passed. Relevant suites were rerun after subsequent
  diagnostics changes. Windows: Qt suite including live snapshot table updates
  passed. CLI acceptance passed on Windows and Linux.
- Storage smoke/soak: real separate processes; provider loss, same-ProviderID
  restart, byte corruption, repair, finalizer restart, application DB rebuild,
  Identity rotation and exact pre/post-rotation file restore all passed.
- [cross-host.json](cross-host.json): 120-second requested workload across
  Windows observer, WSL observer/native loadgen and VPS finalizer/three
  providers. Scheduled provider outage and finalizer restart. Native draining
  runs beyond the requested workload duration. PASS with 344 verified heights,
  8 unique finalized operations (2 AccountCreate, 6 RootPublication), 6
  verified Identity nonces and remote replica target 2.
- [mail.json](mail.json): native Mail run passed with recipient-side exact
  synthetic subject/body checks, using separate Identity Application DBs.
- Both report copies have SHA-256 companions. Original run directories and
  telemetry remain under the Windows Temp `cybou-lab-*` directories; private
  vaults, passwords and ownership tokens were not copied into these artifacts.
- All LAB children were stopped. DEV finalizer/provider services are active on
  29461/29471/29481; latest sampled heights advanced from 74470 to 74480.
  The prior executable is preserved on VPS as
  `build/bin/cybou-node.rollback-before-operator-cli-20260930`.

## Interpretation

Correctness passed for these short runs. This is not a throughput claim:
the cross-host workload submitted only six publications during its requested
window. Storage protection took tens of seconds (three paired samples;
reported p50 about 88.4 s), while one repair sample took about 9.9 s. These
small samples cannot establish a percentile SLA. Synchronous durability/audit
work in the current generator limits its achieved submission rate.

The first cross-host attempt failed when a synthetic Identity submission was
uncertain. Duplicate loopback/public endpoint aliases consumed peer slots.
The accepted run uses one canonical endpoint, initial client synchronization,
and bounded submissions. An early Mail attempt correctly rejected an invalid
synthetic message with a zero timestamp; the generator now supplies the
required application timestamp. Failed reports were retained in their original
run directories rather than rewritten as successful results.

## Remaining scope

This section records the scope at the time of these initial runs. Subsequent
isolated network fault work is documented in the active Operator LAB guide
and separate netem run artifacts.

- 30-minute, two-hour and overnight acceptance runs have not been performed.
  Example manifests support these durations and a bounded submission count.
- Packet loss, injected latency and partitions through an isolated netem
  namespace remain unimplemented. Current controller chaos covers owned
  process loss/restart/crash and scoped blob corruption.
- Cross-host providers in the accepted topology run on VPS. WSL hosts a real
  observer and native client; WSL provider placement behind NAT is not yet
  certified. `--advertise` supports explicit reachable forwarded endpoints.
- Windows resource telemetry currently includes disk free space; CPU/RSS/FD
  samples are available on Linux. Independent bandwidth accounting and
  generalized scheduled workload spikes remain future work.
- Financial load profiles require genuinely funded synthetic vaults and were
  not exercised in the recorded runs.

See [Operator LAB instructions](../../docs/cybou/OPERATOR_LAB.md).
