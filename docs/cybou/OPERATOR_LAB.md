# Operator CLI and Network Lab

This file describes isolated LAB/test processes, not the official DEV VPS
deployment. The target production/DEV bootstrap and desktop-finalizer model is
defined in [`04_NETWORK_LIFECYCLE.md`](04_NETWORK_LIFECYCLE.md).

The operator interface uses named commands of the single `cybou` executable and CYP2. Run `cybou --help`
for the complete grammar. The former positional commands are removed.
`node run` starts the same Full Node on every process, with a local storage
quota and an optional `--poa-key-file`. `doctor` validates the network, key, bind address, peers,
disk space and an isolated copy of an existing canonical DB before startup.
An active DB may change during inspection; stop that LAB process and retry.

## Start a disposable network

Build `cybou` and `cybou-loadgen` with `-DBUILD_TESTS=ON -DCYBOU_ENABLE_LAB_NETWORK=ON`.
LAB runs use `--network lab`: a compiled test-only network with its own fixed public
Network and PoA keys (`cybou network lab-poa-seed --out FILE` writes the finalizer key).
It never uses the DEVNET PoA key. Python 3.11 or later runs the controller.
Copy [lab.example.toml](../../test/stress/lab.example.toml), set absolute binary
paths and give each run fresh roots containing `cybou-lab`.

```sh
python3 tools/cybou_stress.py init /path/to/lab.toml
python3 tools/cybou_stress.py doctor /path/to/lab.toml
python3 tools/cybou_stress.py up /path/to/lab.toml
python3 tools/cybou_stress.py run /path/to/lab.toml
python3 tools/cybou_stress.py status /path/to/lab.toml
python3 tools/cybou_stress.py report /path/to/lab.toml
python3 tools/cybou_stress.py down /path/to/lab.toml
```

`init` exports the compiled LAB PoA seed into isolated test roots. All hosts
use the compiled LAB profile; there is no runtime network-file loader.
This workflow never provisions an official network.
Controller actions require matching ownership markers, scoped paths, ports
at least 30000 and matching process birth identity. They never target DEV
systemd services. `down` retains DBs, vaults, journals, encrypted chunks and
reports. Windows children use hidden consoles for graceful CTRL_BREAK.

The manifest supports `local`, `windows`, `ssh` and `wsl` host transports.
SSH requires `target`, an installed `worker` path and noninteractive access;
WSL requires `distro` and Linux paths. Install this same controller file on
remote hosts together with `cybou_lab_network.py`. All advertised endpoints must actually be reachable from every
participating node. The controller does not configure firewalls or NAT.
For a forwarded provider listener, `--advertise IP:PORT` supplies its reachable
CYP2 endpoint independently of the local `--listen` address. The manifest's
`advertise` field is passed through to the daemon and used in peer files.
The Windows desktop always runs the compiled DEVNET; there is no network file
option. Point it at a LAB peer with an isolated data directory:

```powershell
cybou.exe --datadir C:\cybou-lab\desktop --peer <configured-cyp2-peer>:<port>
```

Diagnostics → Open
Network Monitor reads the desktop's own core snapshot: canonical head and
state root, advertised peer heights, recent operations and private semantic
Mail/Files durability. Peer lag is advisory; it is not finality evidence.
The GUI never consumes controller reports or provider DB enumeration.

## Native workload

`cybou-loadgen` uses production Identity, operation coordinator, publication,
application and storage services. Every synthetic Identity owns its vault and
encrypted application DB. Durable jobs resume after restart. Profiles include
`files`, `mail`, `root-publications`, `payments`, `system-locks` and `mixed`.
The root-publications profile publishes synthetic Files through the existing
RootPublication substrate. Financial profiles require previously funded LAB
Identities; there is no funding or consensus bypass. New onboarding grants
System Balance, not spendable Balance.

The rate is an upper scheduling target, not a promised achieved throughput.
`--max-operations` (manifest `max_operations`) caps submissions while audits
continue for the remaining duration. Example manifests cap at 32 operations
to respect onboarding service funds and local workload limits.
Increase workload only with sufficient real LAB budgets. Use one canonical
endpoint per daemon: duplicate loopback/public aliases consume bounded peer
slots and can prevent new clients from connecting.
Synchronization prefers the configured bootstrap endpoint, then other explicit
routes, before discovered routing hints. HELLO heights are snapshots from
connection time and cannot override that preference. A configured peer that
fails or has no newer blocks still permits fallback to the other peers.
Partial progress also continues through other connected peers until the shared
batch limit is exhausted, so a slowly advancing route cannot hide fresher data.
Startup and draining are outside the requested workload duration. `PASS`
requires current remote durability of all tracked publications and exact
synthetic file download bytes. Local cache is excluded from replica counts.
The default replica target is two; use one explicitly for development runs.
Newly generated Mail also requires the recipient's own ApplicationService to
index the exact synthetic subject/body. These private comparisons stay inside
the native client and never enter public telemetry.

## Evidence and failures

`--event-log FILE` appends output-only `CYBOU_EVENT_V1` JSONL: `v`, random
`run_id`, strictly increasing per-run `seq`, wall-clock `time_ms`, typed
`event` and allowlisted public fields. No plaintext application fields, keys,
mnemonics, file names, message subjects or capsule bodies are accepted as
event fields. Event strings are bounded to 256 bytes and records to 24 fields.
The vocabulary is declared in `src/cybou/event_record.h`; names such as
`sync_complete` are reserved vocabulary and do not imply an implemented
producer. Repeated operation confirmations are deduplicated by OperationID.
Missing chunks and corrupt chunks can both produce `chunk_verify_failed`;
absence alone is not proof of fraud.

Reports include canonical head agreement, height monotonicity, OperationID
height uniqueness, finalized Identity nonce uniqueness, PoA safety halts and
Protected threshold checks. Latency percentiles use paired events from the
same node clock; missing pairs produce null, never a fabricated zero. Reports
are immutable timestamped JSON with SHA-256 companions; `report` replays the
saved timeline.
Completion evidence from older controllers without a frozen convergence
target replays as `INCOMPLETE`; its original immutable report is retained.
Linux process CPU ticks, RSS, FD counts and host disk free space are sampled
separately. Windows currently reports disk free space.
Reports summarize sampled minima/maxima per node. CPU ticks are process
counters, not a utilization percentage; observed maxima are not continuous
peak measurements.
Event collection rejects truncation and logs above 1 GiB. Preserve evidence
and use a fresh LAB directory for the next run; no in-place log rotation.

```sh
python3 tools/cybou_stress.py chaos /path/to/lab.toml provider-loss provider-a
python3 tools/cybou_stress.py chaos /path/to/lab.toml start provider-a
python3 tools/cybou_stress.py chaos /path/to/lab.toml provider-restart provider-a
python3 tools/cybou_stress.py chaos /path/to/lab.toml finalizer-restart finalizer
python3 tools/cybou_stress.py chaos /path/to/lab.toml finalizer-crash finalizer
python3 tools/cybou_stress.py chaos /path/to/lab.toml corruption provider-a --chunk-id HEX
```

For faults during a workload, add `[[chaos]]` entries with `at_seconds`,
`scenario` and `node`; loss/crash entries also accept `duration_seconds`
(1..300, default 10). The controller restarts those exact owned processes
after the specified outage and checks health throughout. An unscheduled down
process during a normal `run` fails its health invariant. The existing
`test/cybou_storage_soak.py` coordinates process loss, corruption, finalizer
restart, application DB rebuild and Identity rotation while a native client
checks repair and exact content.

For latency, packet loss and partition injection, use
[netem.example.toml](../../test/stress/netem.example.toml). Linux requires root
and `ip`/`tc`; WSL supports `user = "root"`, while SSH can use
`sudo_worker = true` with noninteractive sudo. The controller creates a fresh
owned network namespace whose name matches the LAB root basename. Listener
faults match the selected port; full partition affects all namespace TCP.
Every netem action targets only the owned namespace loopback.
It refuses the host namespace, existing foreign namespaces and foreign
processes inside its namespace. All participating nodes must share this
namespace and use loopback endpoints for this isolated topology.

Scheduled scenarios `latency`, `packet-loss`, `listener-partition` and `partition` accept
`duration_seconds` (1..300). Latency uses `delay_ms` (0..10000); packet loss
uses `loss_percent` (0..100). Partition sets loss to 100%.
Partition blocks all TCP traffic inside the owned namespace, including
outgoing peer connections and new connections. It affects every LAB process
in that namespace; the node argument selects the namespace, not a single
isolated node. `listener-partition` instead blocks only the selected listener.
Per-node routed namespace topology is not created by this controller;
the provided manifest tests a network-wide partition.
Only one network fault can be active in a namespace. Reset records final qdisc counters before
removing the fault. After recovery, every manifested node must verify at least
the frozen finalizer height within 120 seconds before the run can pass.
Deadlines are checked between controller polls; transport and collection time
can extend an outage. `chaos.jsonl` records actual apply/reset timestamps.

```sh
python3 tools/cybou_stress.py chaos /path/to/lab.toml latency provider-a --delay-ms 100
python3 tools/cybou_stress.py chaos /path/to/lab.toml network-reset provider-a
python3 tools/cybou_stress.py chaos /path/to/lab.toml partition provider-a
python3 tools/cybou_stress.py chaos /path/to/lab.toml network-reset provider-a
python3 tools/cybou_stress.py down /path/to/lab.toml --force
python3 tools/cybou_stress.py cleanup /path/to/lab.toml
```

`cleanup` removes only the owned namespace after all tracked processes stop;
it retains the LAB data and reports. Do not apply host-wide netem to DEV.

Set `duration_seconds` to 1800, 7200 and 28800 for separate 30-minute, two-hour
and overnight runs, using fresh evidence roots. A short smoke PASS does not
certify these longer runs or cross-host NAT topology.
remains a proposal and grants no finalization or remote chunk admission power.
