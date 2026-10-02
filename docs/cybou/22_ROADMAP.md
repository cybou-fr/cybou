# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over an
encrypted P2P mesh with single-operator hybrid-PQ PoA finality.

## Cleanup and trust-model implementation

1. Remove bootstrap Identity/grant state and `CAP_BOOTSTRAP` from consensus,
   transport and UI. Retain bootstrap as the pinned TLS rendezvous prototype.
2. Remove legacy state decoders and obsolete operation paths; cut over to one
   clean state version after acceptance tests. Keep the current DEV service
   operational until the coordinated cutover.
3. Add official profiles with immutable Network Root `R`, and root-signed
   `OfficialNetworkBinding` plus historical Authority assignments. Separate
   `R` custody and signing purpose from operational PoA `P`.
4. Replace binding verification based on genesis/PoA keys with external `R`.
   Remove bundled production `network.bin` as network truth.
5. Implement crash-safe atomic network-root replacement and full deletion of
   the old network-bound domain. Rotation of `P` must preserve that domain.
6. Remove active Validation protocol/UI scaffolding.

## Integration and acceptance

7. Wire desktop bootstrap locator/client, `EMPTY`/`BOUND` state and root-signed
   DEV genesis creation with activation code.
8. Add bootstrap first-peer discovery followed by direct CYP2 mesh and bounded
   volatile operation relay.
9. Connect Central Authority desktop finalization to the verified assignment,
   one active signer and durable anti-equivocation journal.
10. Validate Windows/Linux clean install, verified sync, recovery, private Mail
    and Files end to end.
11. Harden storage from one development remote replica to two independent Beta
    replicas, with provider audit, repair, restart recovery and soak.
12. Deploy isolated TESTNET, then MAINNET, only after their gates pass.
