#!/usr/bin/env python3
# Copyright (c) 2026 Stanislav Saveliev
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.
"""Multi-process storage soak: real finalizer, three real providers, one client.

The `cybou-storage-soak` client drives the failure sequence and asks this
orchestrator to act on real processes and real disks:

    KILL <port>          kill the provider listening on <port>
    START <port>         start it again on the same data directory
    CORRUPT <port> <id>  flip a byte of that provider's stored blob <id>
    RESTART_FINALIZER    stop and restart the PoA finalizer on its database

Each request is acknowledged by creating WORK/ack-<n>.

Usage: cybou_storage_soak.py CYBOU_NODE CYBOU_STORAGE_SOAK
"""

import os
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

FINALIZER_P2P = 29661
PROVIDERS = {"a": 29671, "b": 29681, "c": 29691}
CAPACITY = str(64 * 1024 * 1024)
TIMEOUT_SECONDS = 1800


def main() -> int:
    node, client = sys.argv[1], sys.argv[2]
    work = Path(tempfile.mkdtemp(prefix="cybou-storage-soak-"))
    # The test-build LAB network: its own Network and PoA keys, never DEVNET's.
    key = work / "finalizer.key"
    subprocess.run([node, "network", "lab-poa-seed", "--out", str(key)], check=True)
    network = "lab"
    peers = work / "peers.txt"
    peers.write_text("".join(f"127.0.0.1 {port}\n" for port in PROVIDERS.values()))

    processes = {}
    logs = {}

    def start(name, args):
        log = open(work / f"{name}.log", "a")
        logs.setdefault(name, []).append(log)
        processes[name] = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)

    def stop(name, kill=False):
        process = processes[name]
        if process.poll() is None:
            process.kill() if kill else process.terminate()
        try:
            process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()

    finalizer_args = [node, "finalizer", "run", "--network", str(network), "--data-dir", str(work / "finalizer-db"),
                      "--key-file", str(key), "--listen", f"127.0.0.1:{FINALIZER_P2P}", "--block-interval", "200ms", "--peers", str(peers),
                      "--peer-admission", "lab"]

    def provider_args(name):
        return [node, "provider", "run", "--network", str(network), "--data-dir", str(work / f"provider-{name}-db"),
                "--peer", f"127.0.0.1:{FINALIZER_P2P}", "--listen", f"127.0.0.1:{PROVIDERS[name]}", "--capacity", CAPACITY,
                "--peer-admission", "lab"]

    def provider_by_port(port):
        return next(name for name, p in PROVIDERS.items() if p == port)

    start("finalizer", finalizer_args)
    time.sleep(2)
    for name in PROVIDERS:
        start(f"provider-{name}", provider_args(name))

    client_work = work / "client"
    smoke = subprocess.Popen([client, str(network), str(client_work), "127.0.0.1", str(FINALIZER_P2P)],
                             stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    deadline = threading.Timer(TIMEOUT_SECONDS, smoke.kill)
    deadline.start()
    result = 1
    try:
        for line in smoke.stdout:
            line = line.rstrip()
            print(f"[client] {line}", flush=True)
            if not line.startswith("REQUEST "):
                continue
            _, n, action, *args = line.split()
            if action == "KILL":
                name = provider_by_port(int(args[0]))
                stop(f"provider-{name}", kill=True)
                print(f"[soak] killed provider-{name}", flush=True)
            elif action == "START":
                name = provider_by_port(int(args[0]))
                start(f"provider-{name}", provider_args(name))
                print(f"[soak] started provider-{name}", flush=True)
            elif action == "CORRUPT":
                name = provider_by_port(int(args[0]))
                chunk = args[1]
                blob = work / f"provider-{name}-db.chunks" / "chunks" / chunk[:2] / chunk[2:4] / chunk
                data = bytearray(blob.read_bytes())
                data[len(data) // 2] ^= 0x01
                blob.write_bytes(bytes(data))
                print(f"[soak] corrupted {chunk[:16]}... on provider-{name}", flush=True)
            elif action == "RESTART_FINALIZER":
                stop("finalizer")
                start("finalizer", finalizer_args)
                print("[soak] restarted finalizer", flush=True)
            else:
                print(f"[soak] unknown request {action}", flush=True)
                break
            (client_work / f"ack-{n}").touch()
        result = smoke.wait()
    finally:
        deadline.cancel()
        for name in list(processes):
            stop(name)
        for files in logs.values():
            for log in files:
                log.close()
    if result != 0:
        for name in processes:
            print(f"----- {name} log (tail) -----")
            print("".join((work / f"{name}.log").read_text(errors="replace").splitlines(True)[-40:]))
        print("storage soak FAILED")
        return 1
    print("storage soak passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
