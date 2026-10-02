# CYBOU protocol and product roadmap

CYBOU is an Identity-centered private Mail and Files platform over a
content-addressed encrypted P2P mesh, finalized by a single-operator hybrid-PQ PoA Authority.

## Roadmap phases

1. **Finish official DEV bootstrap lifecycle**
   - Connect desktop to pinned DEV bootstrap locator (`51.255.46.58:29461`).
   - Query `STATUS` (`EMPTY` vs `BOUND`).
   - Implement client handling for `BOUND` generation verification and full wipe on newer generation.

2. **Create DEV network from desktop**
   - Desktop command/wizard to create DEV genesis with initial Authority key $K_0$.
   - Claim `EMPTY` bootstrap with one-use activation code.
   - Transition DEV bootstrap to `BOUND` at generation 1.

3. **Bootstrap → first peer discovery**
   - Retrieve initial peer addresses from bootstrap rendezvous.
   - Establish direct CYP2 mesh between nodes.
   - Relay blocks and operations peer-to-peer without routing through bootstrap.

4. **Central Authority operator workflow**
   - Operate hybrid-PQ PoA block finalization from the Central Authority desktop.
   - Maintain anti-equivocation journal and fail-closed safety halt.

5. **Windows and Linux desktop acceptance**
   - Validate clean-install UX, Identity creation, and network synchronization.
   - Verify private Mail sending/receiving and Files upload/download.

6. **Storage durability hardening**
   - Advance from development target (1 remote full replica) to Beta target (2 independent remote full replicas).
   - Multi-process failure soak, restart recovery, and provider audit/repair.

7. **TESTNET release**
   - Staging network deployment with isolated bootstrap and testing Authority key.

8. **MAINNET release**
   - Production network deployment under the French sovereign P2P policy.
