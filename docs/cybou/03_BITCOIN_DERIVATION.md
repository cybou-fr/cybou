# 03 — Bitcoin derivation strategy

Bitcoin Core is a bootstrap codebase, not a compatibility target.

## Keep initially

Preserve as much as possible until a milestone proves replacement is required:

- C++ build structure;
- core validation patterns;
- peer management concepts;
- connection management;
- address manager concepts;
- UTXO machinery;
- transaction/mempool architecture;
- persistence abstractions;
- Qt desktop scaffolding;
- testing infrastructure.

## Replace progressively

CYBOU will eventually replace or heavily modify:

- Bitcoin network parameters;
- Bitcoin genesis;
- Bitcoin network discovery;
- Bitcoin address presentation;
- PoW/mining consensus;
- Bitcoin-specific difficulty logic;
- long-history IBD assumptions;
- Bitcoin-specific wallet/account UX;
- fixed cryptographic assumptions;
- Bitcoin Script semantics where they no longer fit;
- Bitcoin-specific RPC/UI features not needed by CYBOU.

## Remove from the desktop product

The CYBOU desktop product must not expose:

- JSON-RPC;
- REST;
- local HTTP control server;
- mining RPC;
- Bitcoin-network selection;
- Bitcoin bootstrap infrastructure.

Internal C++ interfaces replace application RPC.

## Migration rule

Do not perform a mass rewrite.

Each replacement is its own milestone with:

1. design document;
2. code change;
3. automated tests;
4. desktop manual acceptance;
5. frozen result before the next replacement.
