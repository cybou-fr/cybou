# Beta Wallet uncertain-delivery acceptance — WALLET-RESTART-01

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: prepared manual/live scenario; NOT RUN. Follow the green-revision gate
in [the acceptance plan](DESKTOP_BETA_ACCEPTANCE_PLAN.md). This document does
not authorize a payment, funding, journal modification or signer interruption.

## Preconditions and evidence

Use two operator-controlled ordinary DEVNET Identities, with sufficient Available
Balance and Network balance for one agreed whole-number payment and its fee.
Use a sender client without the active PoA signer; keep the sole signer/history
intact elsewhere. Resolve other account operations and pending rotation first.
Record source/app hash, binding, OS/language/theme, finalized baseline height,
sender/recipient balances and expected payment/fee. Redact account labels in
public evidence; never record passwords, mnemonic or private journal contents.

Prepare a way to interrupt only the sender's mesh connection after submission
but before its outcome is known. Disconnecting at an arbitrary time does not
prove this window was reached. Existing bounded read-only diagnostics or a
reviewed test-build trace must establish unique OperationID, signed-operation
byte fingerprint and coordinator state before/after restart. If that evidence
is unavailable, mark the corresponding criterion unverified; do not fabricate
it from an unchanged amount or ledger label.

## Procedure and expected results

| Step | Action | Pass condition |
|---|---|---|
| 1 | Review resolved recipient, amount and fee; confirm exactly once | One durable operation intent; one OperationID; GUI remains responsive |
| 2 | Interrupt the sender connection in the defined uncertainty window | Evidence establishes submission with unknown outcome; UI distinguishes checking/pending from final rejection |
| 3 | Attempt to start another payment while unresolved | No second account operation is signed/submitted; coordinator explains the outstanding operation |
| 4 | Close the ordinary sender normally and restart the same binary while offline | Recoverable pending intent remains; same Identity and original operation evidence; no replacement signature |
| 5 | Reconnect and allow normal reconciliation/retry | Same OperationID and exact signed-operation bytes; retry does not create another payment |
| 6 | Observe a locally verified PoA-finalized outcome on both clients | For the accepted payment: one transfer of the reviewed amount and one protocol fee, each on its correct balance side |
| 7 | Restart once more and inspect finalized ledger/state | Finalized outcome persists; no duplicate transfer, fee or contradictory pending entry |

An acknowledgment is not finality. The finalizer may have included the payment
while the sender was offline; unchanged displayed balances are not required
once verified finality arrives. Account for unrelated finalized credits, rent or
settlements before attributing balance changes to this operation. A valid payment
that is ultimately rejected needs a separate recorded failure outcome; do not
count it as the successful-transfer path above.

## Result record

- Outcome: NOT RUN / PASS / FAIL / BLOCKED / INCONCLUSIVE; UTC start/end:
- Exact source/app hash; configuration; clean/dirty status:
- OS/language/theme; physical or programmatic input; redacted client topology:
- DEVNET binding; baseline/final heights; signer continuity:
- Reviewed amount/fee; baseline/final balance reconciliation:
- Original/recovered/final OperationID and signed-byte fingerprint evidence:
- Proof of uncertainty window; interruption/restart/reconnect timestamps:
- Per-step result; redacted logs/artifact hashes; unresolved limitations:

PASS requires demonstrated uncertainty, exact-byte continuity and exactly one
finalized transfer/fee. Missing the uncertainty window is INCONCLUSIVE, not PASS.
Preserve failed attempts when rerunning. This does not prove crash/power-loss
recovery, Identity rotation or independent storage durability.
