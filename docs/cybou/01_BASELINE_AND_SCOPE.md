# 01 — Baseline and scope

## Starting repository

The repository already contains a Bitcoin Core copy under the CYBOU project directory.

Before functional modification, record:

- exact upstream release/tag/commit;
- source archive or Git origin;
- build tool versions;
- compiler versions;
- Qt version;
- successful clean Windows build;
- successful upstream automated tests practical in the local environment.

Tag this state locally, for example:

```text
upstream-bitcoin-baseline
```

Do not continuously merge Bitcoin `master` after CYBOU consensus divergence. Review/cherry-pick relevant upstream fixes intentionally.

## Early product scope

1. quarantine Bitcoin networks;
2. establish CYBOU genesis/devnet;
3. introduce first-class CYBOU protocol operations;
4. establish AccountID/operator authority;
5. implement E2E typed MailTx;
6. establish bounded deterministic state/snapshot;
7. establish formal BFT finality;
8. ship CYBOU Email pilot;
9. build Object Storage before mass-scale mail/attachments;
10. build Backup/Drive later.

## Consumer process model

```text
cybou.exe
├── Qt UI
└── NodeCore
    ├── P2P
    ├── typed protocol operations
    ├── chain/history
    ├── bounded current state
    ├── BFT consensus
    ├── wallet / identity
    ├── Mail protocol
    ├── Proof of Trust
    ├── Object Storage later
    ├── Backup later
    └── Drive later
```

No required desktop daemon/API split.

An infrastructure/VPS build may reuse NodeCore without GUI, but must not become a privileged central data proxy.
