# Beta Identity session acceptance — ID-SESSION-01

Status: prepared manual/live scenario; NOT RUN. This is a small part of
[the active acceptance plan](DESKTOP_BETA_ACCEPTANCE_PLAN.md), not clean-machine
restore, key rotation or release approval. Execute only after the applicable
revision/CI gate is green. Preparation does not authorize restarting a signer.

## Preconditions

Use an existing ordinary Identity on a client without the active PoA signer.
Keep the sole signer and its durable history running elsewhere. Record the exact
app revision/hash and disable fixture/screenshot environment variables. Resolve
unsaved editor content before the test; do not begin with an unresolved operation
or pending rotation. Do not delete state, vaults, caches or signing journals.

Record language, theme, OS, physical DPI and whether inputs are physical or
programmatic. Compare private baseline AccountID/name and acknowledged semantic
Mail/Files items locally; public evidence should use redacted labels. No recovery
words, passwords or private content belong in logs or screenshots.

## Procedure and expected results

| Step | Action | Pass condition |
|---|---|---|
| 1 | Open the existing Identity using its vault password | Correct Identity opens; preparation/Ready status matches actual local work |
| 2 | Open an own item and private Console output, then lock | Private details, editor/session diagnostics, output/history and completion clear; public node observations may remain |
| 3 | Make one incorrect password attempt | Unlock is rejected; private views remain inaccessible; error does not expose secrets |
| 4 | Unlock with the correct password | Same AccountID/name; acknowledged Mail/Files remain usable; no create/rotate/transfer is triggered |
| 5 | Close the ordinary client normally, restart the same binary, unlock if requested | Existing vault opens and semantic items reload; no duplicate Identity or operation is submitted |
| 6 | Inspect Wallet and task/session state | Ledger reflects verified finalized state; volatile task rows need not survive restart; no stale private output reappears |

Balance changes must be explained by actual finalized operations, not treated
as a failure merely because the network advanced. Peer sync is a local liveness
hint, not proof of globally current state. Record actual startup lock behavior;
this scenario does not prescribe new automatic-unlock policy. If the client owns
the active signer, stop here and prepare a separate coordinated operator scenario.

## Result record — complete after execution

- Outcome: NOT RUN / PASS / FAIL / BLOCKED; UTC start/end:
- Exact source revision; app SHA-256; configuration and clean/dirty status:
- OS; language/theme; DPI/monitor setup; physical/programmatic input:
- DEVNET binding; redacted client/signer locations; signer continuity evidence:
- Each step: observed result, elapsed time, relevant redacted artifact:
- Private baseline comparison: same Identity and retained acknowledged items:
- Unexpected finalized changes or duplicate-operation evidence:
- Failure/reproduction notes; unresolved scope; reviewer:

PASS requires every applicable step and its evidence. A failed step remains
recorded when rerun. This does not prove recovery on another machine, independent
remote replicas, physical accessibility or uninterrupted signing safety.
