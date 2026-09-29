CYBOU documentation overlay

Prepared against repository:
  cybou-fr/cybou
  main
  HEAD: e28c60fd673a6d97da963c156e72fc94063f3ecf

Purpose:
  Consolidate the current CYBOU architecture for the application data plane,
  Mail/Files private indexing, clean-machine recovery, storage durability,
  and Identity Authority.

Usage:
  Extract this archive at the repository root and allow files to overwrite.

This overlay intentionally contains documentation/specification files only.
It does not contain source-code, build, deployment, genesis, or runtime changes.

Large existing UX documents 82_MAIL_UI_UX.md and 83_STORAGE_UI_UX.md are not
replaced: their finality-first state model remains valid. The durability target
is now frozen in STORAGE_ADMISSION.md and 81_BETA_PRODUCT_SCOPE.md:
  development: 2 independent remote full replicas
  Beta:        3 independent remote full replicas
The local encrypted copy does not count toward remote durability.

The existing PoT file paths are retained to avoid link churn, but their content
now defines Identity Authority and states that Authority supersedes the earlier
Proof-of-Trust concept.
