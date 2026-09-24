# 77 — `.cybou` name registry

Status: commit, work, reveal, and ownership rules are implemented in the
canonical core. Desktop claiming and operational DEV cutover remain open.

The label before `.cybou` is exactly 5–32 ASCII lowercase bytes from
`[a-z0-9-]`; first and last are alphanumeric. Reject uppercase, Unicode,
normalization aliases, `--`, `xn--` prefix, and all-digit labels. Lengths 1–4
are permanently reserved. Initial reserved labels include `cybou`, `admin`,
`root`, `system`, `support`, `security`, `operator`, `validator`, `wallet`, and
`mail`. Freeze the complete list and registry version in immutable network
parameters; never silently extend them on an existing network.

One AccountID may own one primary name. Claiming is free in CYBOU but requires
separate anti-Sybil work. The initial registry has no sale, transfer, expiry, or recycling.
Names are pseudonymous aliases, not civil identity proof.

## Commit → work → reveal

1. Generate a secret random salt and commit to canonical label, AccountID,
   NetworkID, and registry version with a domain-separated hash.
2. Finalize NameCommit before accepting work or reveal. Save the salt in the
   encrypted vault before broadcasting the commit.
3. NameClaimWork binds NetworkID, AccountID, commitment, deterministic
   height-derived epoch, and nonce. Difficulty and limits are network
   parameters; local wall clock is irrelevant.
4. NameReveal discloses label and salt. Consensus checks canonical syntax,
   reservations, commitment, work, commit finality, account ownership, and
   absence of an already finalized owner.
5. First valid finalized reveal in canonical block/operation order wins.

Before activation, freeze commit lifetime, minimum finality depth, work
target, salt length, operation bounds, pending-commit cap, and fee policy in
the network definition. Test mempool copying, competing reveals, cross-network
replay, Unicode/confusable rejection, and interruption during every phase.
