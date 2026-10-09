# DEVNET live content acceptance — 2026-10-04

Status: EVIDENCE
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

This exercise uses the existing compiled DEVNET after the explicitly authorized
coordinated development reset. Network keys, signed genesis and the initial TLS
pin are unchanged. Windows desktop is the sole PoA signer. The OVH VPS runs an
ordinary Full Node; Windows and WSL clients use production application services,
ordinary fees, France-only admission and finality-first storage admission.

## Observed results

| Exercise | Result | Scope |
| --- | --- | --- |
| WSL storage smoke | PASS | 716,800 plaintext bytes recovered from the VPS after removing local encrypted leaves; exact decrypted bytes checked. |
| Mail publication and delivery | PASS | Two independent synthetic Identity services in WSL; two successful rounds, each with one message in each direction. Subject and body checked at each recipient. |
| Mail recovery | PASS | Both clients' private application indexes and all local encrypted payload files removed; each recovered two incoming messages from the VPS. Recovery submitted zero operations. |
| Windows Files publication | PASS | Two synthetic Identity services, each publishing one 4,194,304-byte file; one remote replica per required chunk confirmed. |
| Windows Files clean recovery | PASS | Both private application indexes and all local encrypted payload files removed again; catalogs, exact plaintext bytes and protected remote placement recovered from the VPS. Recovery submitted zero operations. |

Evicted test data remains outside the active client directories in ignored local
archives. Vaults and finalized chain data were retained. The recovery clients do
not read the archives. All remote content in this exercise is synthetic.

## Defects found and repaired

- Relay acknowledgement previously removed a candidate from the global queue.
  An author's own poll could therefore consume it before PoA received it.
  Commit `022eaa1` retains candidates until finality and tracks delivery per session.
- Unrelated OpenSSL errors could turn nonblocking TLS waits into false fatal
  errors. Commit `0723371` clears the thread error queue before TLS handshake,
  read and write calls. A regression fails without this repair and passes with it.
- Placement rebuilding previously discarded verified progress when a later
  provider proof was unavailable. It now stores progress separately in the
  encrypted Application DB, resumes after interruption and checks the complete
  authorization root before exposing a protected placement. Partial progress is
  excluded from normal placement indexes and durability summaries.
- The synthetic client emitted an unsupported event field and audited too
  aggressively. It now emits accepted fields, bounds periodic audit traffic and
  retries unavailable chunks while still rejecting incorrect decrypted content.
  Its `recovery` profile requires existing finalized test Identities and checks
  recovered content without generating new operations.

Final Windows core verification: **232 tests, 9,441 assertions, all passed**.
The incremental-rebuild regression limits transport to one proof per attempt,
reopens the service and private DB between attempts and checks that incomplete
progress never appears as protected durability.

## Transport condition and limits

Large VPS-to-Windows packets repeatedly failed on the current mobile connection,
while smaller packets and WSL transfers worked. A temporary VPS host route to
the client's current public IP with MTU 1280 allowed Windows recovery to finish.
This is a local transport workaround, not a protocol exemption, authority route,
Geo bypass or persistent deployment setting. The test outcome is conditional
on that workaround; normal-MTU operation on this link is not established.

DEV remote target is one full replica. This exercise establishes actual remote
storage and recovery with one VPS, not Beta's two independent remote replicas,
multi-provider failover, a long-running availability guarantee or GUI acceptance
of the Mail/Files flows. The two synthetic recipients are distinct Identities,
not two physically independent recipient machines.

The canonical fee schedule was exercised without special funding. Each 4 MiB
file occupied 19 encrypted chunks including its private manifest: the chunk
component alone costs 76 CYBOU, plus the canonical-operation byte component.
Onboarding's 6,000 CYBOU service budget and its nominal 5 GiB storage allowance
are different limits. The budget cannot pay the minimum 40,960 CYBOU chunk fee
for an entire 5 GiB allowance. Protocol fees go to Central Authority; this
exercise establishes no provider payment or economic incentive for third-party
storage. Those remain constraints on the product's storage proposition.

## Geo updater verification (2026-10-04)

A fresh Windows Full Node automatically downloaded the October DB-IP country
dataset into an empty Geo cache and then connected to the DEV VPS and synchronized
finalized blocks. The published SHA-1 `c8ad1dfe98cb29bcacd82a5c3ec361c880e5bb59`
matches the decompressed CSV, not the gzip envelope. The installed CSV has
SHA-256 `53864a68fbfef02c27c717d08d431a6d453e9e00e77d9e8da841d382267fca88`.
No pre-existing dataset was copied into this test node.

## Recovery commands

Use the same existing test directories and passwords as the publication run.
After stopping the clients, move only their private application index and local
chunk payload directories outside the active paths. Preserve their vaults and
finalized state. Never use this procedure on operator data as a routine reset.

```text
cybou-loadgen --network devnet --data-dir TEST_CLIENTS --peer 51.255.46.58:29461
  --password-file TEST_PASSWORD --identities 2 --profile recovery
  --expected-incoming-mail 2 --replicas 1 --drain-timeout 180s

cybou-loadgen --network devnet --data-dir TEST_CLIENTS --peer 51.255.46.58:29461
  --password-file TEST_PASSWORD --identities 2 --profile recovery
  --expected-files 1 --replicas 1 --drain-timeout 180s
```

Each command is written on multiple lines for readability. Join its arguments
or use the line continuation syntax of the chosen shell.
