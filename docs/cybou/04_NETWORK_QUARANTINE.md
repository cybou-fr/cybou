# 04 — Bitcoin network quarantine

This is the first functional CYBOU patch and precedes the first normal/manual desktop launch.

## Objective

A CYBOU development build must be structurally unable to initiate Bitcoin block download.

"Do not download much Bitcoin" is insufficient.

The desired invariant is:

> A manually launched `cybou.exe` cannot select, discover or join Bitcoin mainnet/testnet/testnet4/signet.

## Required quarantine work

Remove or disable in CYBOU builds:

- Bitcoin mainnet selection;
- Bitcoin testnet/testnet4 selection;
- signet selection;
- Bitcoin DNS seeds;
- Bitcoin fixed seeds;
- Bitcoin-specific default ports;
- Bitcoin mainnet/test network magic use;
- Bitcoin default datadir reuse;
- Bitcoin genesis selection;
- automatic paths that can invoke Bitcoin IBD.

Create dedicated CYBOU development parameters:

```text
network: CYBOU-DEV
datadir: dedicated CYBOU path
magic:   CYBOU-specific
ports:   CYBOU-specific
genesis: CYBOU development genesis
```

The final values are frozen before public genesis, not during the first quarantine patch.

## v0.0.1 acceptance invariant

During a normal manual run:

```text
Bitcoin peers                  = 0
Bitcoin DNS seed requests      = 0
Bitcoin fixed-seed connections = 0
Bitcoin blocks downloaded      = 0 bytes
```

The untouched upstream baseline may be compiled and tested in isolation, but it is not the manual CYBOU application milestone.
