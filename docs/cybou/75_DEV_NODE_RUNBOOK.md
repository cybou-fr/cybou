# 75 — DEV authority node and observer runbook

`cybou-node` is an experimental standalone executable for the disposable
CYBOU DEV state chain. It does not use the inherited Bitcoin PoW chain.

## Network setup

Create a 32-byte raw Ed25519 validator seed and keep it private. On Windows,
PowerShell can create one with a cryptographic random source:

```powershell
$key = New-Object byte[] 32
[System.Security.Cryptography.RandomNumberGenerator]::Fill($key)
[System.IO.File]::WriteAllBytes('validator.key', $key)
```

Restrict access to that key file before using it on a shared machine. The
public key printed by `pubkey` is in CYBOU's `uint256` display order:

```text
cybou-node pubkey validator.key
cybou-node init-dev network.bin VALIDATOR_PUBLIC_KEY_HEX
```

Copy `network.bin` to the observer by a trusted channel. It contains the
immutable network definition and genesis state, not the private key. The
network ID printed by `init-dev` should match on all nodes. `init-dev` refuses
to overwrite an existing network file.

## Producer and observer

```text
cybou-node serve network.bin producer-db validator.key 127.0.0.1 29460 1000
cybou-node sync network.bin observer-db 127.0.0.1 29460 5
```

The final `serve` argument is the block interval in milliseconds, default
1000. `sync` applies the specified count of new blocks, retries unavailable
heights, and exits with an error if no verified block arrives for 30 seconds.
Both commands reopen the same database after restart and reject a database
bound to another network definition. The producer validates its key before
opening the listener. Stop the producer with Ctrl+C for a graceful exit.

The listener handles one bounded request at a time. The observer verifies
every block before committing it. A trusted genesis file is essential: the
block feed does not negotiate network identity or bootstrap trust.

## Current limits

This DEV process produces empty blocks. It has no operation-submission API,
peer discovery, snapshot sync, authentication, encrypted transport, or desktop
client connection. Expose the listener only inside a trusted private network
for now. The standalone `init-dev` profile has no Operator Authority keyset,
so it cannot admit additional validators. It is separate from future Beta and
Mainnet genesis and economics.
