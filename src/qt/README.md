# CYBOU Qt desktop

The native CYBOU desktop is built from `cybou_qt`, the `cybou` executable, and
the smoke test target `test_cybou-native-qt`. Bitcoin Qt UI, forms, translation
catalogs, and their test harness are not part of this application.

Build and launch instructions are in the root project README and CMake
presets. Qt translation catalogs will be added when CYBOU's own localization
workflow is defined.

## Mail and Files data flow

```text
Pages -> CybouDesktopModel -> CybouApplicationBackend
                                ├── CybouFixtureApplicationBackend  (CYBOU_UI_FIXTURE)
                                └── CybouCoreApplicationAdapter     (live Mail and Files; see CORE_INTEGRATION.md)
```

- Pages call `CybouDesktopModel::request*` for every persistent Mail/Files
  action. The model forwards one command to the backend and changes its
  Mail/Files projection only when the backend reports the result. Sending and
  uploading show an optimistic `Preparing` item; the backend owns every later
  state, and Sent is shown only for `Protected` messages.
- Without a backend, `featureAvailability().mail/files` stay false whatever is
  requested. In live mode `CybouCoreApplicationAdapter` serves Mail and
  Files.
- The model opens the backend's Identity session while the Identity is
  unlocked and clears the private projection (and so global search) when it
  locks.
- Protection (`CybouContentState`), retrieval (`CybouRetrievalState`) and
  local availability (`available_offline`) are independent.
