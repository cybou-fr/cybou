# 29 — Documentation authority and lifecycle

CYBOU inherits a large codebase whose documentation will become partially wrong over time.

This document prevents "documentation split-brain".

## Authority order

From strongest to weakest:

```text
1. Frozen CYBOU decisions (`24_DECISIONS.md`)
2. CYBOU consensus/protocol specifications
3. CYBOU security invariants
4. CYBOU implementation architecture docs
5. CYBOU milestone/manual acceptance docs
6. Reviewed inherited Bitcoin docs marked KEEP/ADAPT
7. Transitional Bitcoin docs
8. Archived/unreviewed upstream Bitcoin docs
```

An upstream Bitcoin document may not override a frozen CYBOU decision.

## Status header for inherited documents

Until all upstream docs are migrated, any inherited document that remains in active `doc/` should eventually receive a short CYBOU status header, conceptually:

```markdown
> CYBOU migration status: TRANSITIONAL
> This document describes inherited Bitcoin Core behavior.
> Do not treat Bitcoin network/RPC/mining assumptions as CYBOU requirements.
> See docs/cybou/28_BITCOIN_DOCUMENTATION_MIGRATION.md.
```

Do this selectively; avoid creating a huge noisy patch before network quarantine.

## No duplicate normative text

Do not copy consensus rules into multiple documents.

Prefer:

```text
one normative spec
    -> other docs link to it
```

This is especially important for:

- consensus quorum;
- storage 3:1 formula;
- state roots;
- name normalization;
- fee calculation;
- crypto suites;
- serialization bounds.

## Decision changes

A frozen decision changes only by:

1. documenting why;
2. updating `24_DECISIONS.md`;
3. updating affected specs;
4. updating manual acceptance;
5. updating `spec/cybou_baseline.yaml`;
6. adding migration notes if protocol state is affected.

## Documentation review in every milestone

Every milestone asks:

```text
Which inherited Bitcoin documents became false because of this code change?
Which CYBOU documents must become authoritative now?
Which transitional docs can now be archived/removed?
```

Documentation migration is part of Definition of Done.
