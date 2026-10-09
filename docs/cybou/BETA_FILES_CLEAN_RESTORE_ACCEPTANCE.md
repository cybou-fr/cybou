# Beta clean-client Files recovery — FILES-RESTORE-01

Status: CURRENT
Scope: Classification only; dated evidence and pending requirements retain their stated limits. Content review follows the documentation refactor plan.

Recorded status: prepared manual/live scenario; NOT RUN. Follow the green-revision and
independent-topology gates in [the acceptance plan](DESKTOP_BETA_ACCEPTANCE_PLAN.md).
Preparation does not authorize vault export, deletion, network reset or signer
interruption. Provider-outage repair is a separate scenario.

## Preconditions

Use an existing ordinary Identity on source client S and a separate clean physical
client R. Keep the sole PoA signer/history elsewhere and two independently
documented remote full replicas available. Colocated processes/Windows–WSL are
not independent-host evidence. Paid placement remains service-owned.

Select one non-sensitive test file and retain the original outside CYBOU for a
local byte/digest comparison. Keep the current recovery phrase available through
the existing private handling process; never put it into evidence artifacts.
R must have no copied vault, Application DB, encrypted chunks or previous cache
for this Identity/content. Record exact app/source hashes, binding and redacted
physical topology. A new folder on S is not a clean-machine substitute.

## Procedure and expected results

| Step | Action | Pass condition |
|---|---|---|
| 1 | Upload the test file on S | Local staging and pending publication remain distinct from remote protection |
| 2 | Wait for verified publication finality and the required replicas | Scoped evidence confirms full remote copies and their actual independent hosts; Protected is not inferred from finality alone |
| 3 | Close S normally and keep it unavailable throughout recovery | The source client cannot serve content; signer/providers continue separately |
| 4 | Start R without fixture variables and restore using the current phrase and a local vault password | Same AccountID/current key epoch; local vault opens; restore creates no AccountCreate/IdentityRotate operation |
| 5 | Let accessible publication history and encrypted catalog rebuild | Test file/folder metadata becomes visible through authorized application data; Ready does not imply download or remote durability |
| 6 | Download the test file on R | Actual remote retrieval, verification of ChunkIDs over exact encrypted bytes and successful local decryption |
| 7 | Compare the recovered file with the retained original, then restart R | Exact plaintext bytes/length match; restored vault and acknowledged catalog remain usable after restart |

Verify recovery through the owner self-capsule/application path, not a copied
local index. Record the publication/reference associated with the recovered item;
do not require identical device-local row IDs. Missing metadata must remain
pending/unknown or show an actionable error, not fabricated complete recovery.
Do not copy cache/data from S or alter journals to make the test pass.

A cached read or filename match cannot prove remote retrieval or byte fidelity.
Use bounded authorized diagnostics for retrieval/integrity evidence; unavailable
evidence is unverified. Compare plaintext/digests locally and publish only the
reviewed redacted result. Keep finalized state, accessible history, downloaded
bytes and measured replica protection as separate observations.

## Result record

- Outcome: NOT RUN / PASS / FAIL / BLOCKED; UTC start/end:
- Source/app hashes; configuration; OS/language/theme/input scope:
- DEVNET binding; redacted S/R/provider hosts and independence boundaries:
- Proof R was clean; proof S stayed unavailable; signer continuity:
- Upload/publication ID and finalized height; full-copy evidence scope/time:
- Same Identity/current epoch comparison; recovered catalog/reference evidence:
- Remote retrieval/ChunkID verification; exact local plaintext comparison:
- Post-restart result; per-step logs/artifact hashes; unresolved limitations:

PASS requires clean-client reconstruction, demonstrated remote retrieval and
exact-byte recovery with S unavailable. Retain failed attempts on rerun. This
scenario does not prove historical-key recovery after rotation, recovery of local
unsent drafts/preferences, provider-outage repair or hidden-copy erasure.
