# Beta offline Mail attachment acceptance — MAIL-OFFLINE-01

Status: prepared manual/live scenario; NOT RUN. Execute after the applicable
green-revision and independent-topology gates in
[the acceptance plan](DESKTOP_BETA_ACCEPTANCE_PLAN.md). Preparing this scenario
does not authorize sending Mail, permanent deletion or stopping a signer.

## Preconditions

Use two operator-controlled ordinary client Identities on separate physical
hosts, with finalized names, acceptable recipient encryption keys and adequate
service budget. Keep the sole PoA signer/history elsewhere. Beta evidence needs
two independently documented remote full replicas; colocated VPS processes or
Windows/WSL cannot establish that independence. Placement remains service-owned.

Use one non-sensitive test attachment with a locally retained original and digest.
Record source/app hashes, binding, OS/language/theme, physical/programmatic input
and redacted topology. Keep recipient B disconnected until A's send completes;
B must not already have this attachment cached. No plaintext/private keys or
recovery words belong in published evidence.

## Procedure and expected results

| Step | Action | Pass condition |
|---|---|---|
| 1 | Verify B is offline; A composes one uniquely identifiable test message with the attachment | Correct recipient and attachment; recoverable local draft/preparation |
| 2 | A confirms Send once | UI remains responsive; one outgoing intent; no premature Sent claim |
| 3 | Observe A's verified publication and required replica evidence | Sent follows verified finality and confirmed required durability; record scope/time, not only a badge |
| 4 | Close ordinary client A; reconnect B | A's client is unavailable; B indexes the relevant verified history and discovers/decrypts the message |
| 5 | Retrieve the attachment on B and compare locally with the original | Verified encrypted ChunkIDs, successful decryption and identical plaintext bytes/digest |
| 6 | B uses Save to Files once and waits for durable acknowledgment | An independent Files catalog/retention reference exists; reuse avoids duplicate encrypted-content upload where permitted |
| 7 | Delete the test Mail copy using the approved permanent-deletion path, then restart B | Deletion is acknowledged; the saved Files item persists and its downloaded bytes still match |

Keep deletion of the recipient's Mail copy distinct from author revocation of the
publication. Trash alone does not establish permanent-deletion retention. If no
supported permanent-deletion path/evidence is available, record that step as
BLOCKED rather than treating Trash as a substitute. Do not revoke the author's
publication or manually purge chunks to force this scenario.

A successful cached read does not prove remote availability. Record whether each
retrieval used local or remote bytes; unknown source remains unknown. Metadata
or reference creation is not durable content by itself. Record actual reusable
publication/content identifiers through bounded authorized diagnostics; if reuse
cannot be measured, do not infer it from filename or transfer speed.

## Result record

- Outcome: NOT RUN / PASS / FAIL / BLOCKED; UTC start/end:
- Source/app hashes; configuration; OS/language/theme/input scope:
- Binding; redacted client/provider domains; signer continuity:
- Proof B was offline and lacked the test attachment; A unavailable at retrieval:
- Send intent/publication identifiers; finalized height; replica count/scope/time:
- Attachment verification and local digest comparison; retrieval source:
- Saved Files reference/acknowledgment and measured reuse evidence:
- Mail deletion scope/acknowledgment; post-restart Files retrieval result:
- Per-step redacted logs/artifact hashes; failure/reproduction and limitations:

PASS requires every applicable outcome with evidence. Keep failed attempts on
rerun. This does not establish clean-machine recovery, author-revocation retention,
hidden-copy erasure, provider-outage repair or physical drag/drop acceptance.
