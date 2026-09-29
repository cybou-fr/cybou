#!/usr/bin/env python3
# Copyright (c) 2026 Stanislav Saveliev
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/license/mit/.
"""Multi-process PoA + storage provider smoke test.

Starts a real PoA finalizer, two real storage providers (`cybou-node provide`)
and the `cybou-storage-smoke` client, all as separate processes on loopback:

    block production -> verified sync -> Identity -> RootPublication
    -> finality -> authorized chunk PUT -> remote durability
    -> provider loss -> audit -> repair -> remote GET of exact bytes

Usage: cybou_storage_smoke.py CYBOU_NODE CYBOU_STORAGE_SMOKE
"""

import os
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path

FINALIZER_FEED = 29560
FINALIZER_P2P = 29561
PROVIDERS = {"a": 29571, "b": 29581}
CAPACITY = str(64 * 1024 * 1024)
TIMEOUT_SECONDS = 600


def main() -> int:
    node, client = sys.argv[1], sys.argv[2]
    work = Path(tempfile.mkdtemp(prefix="cybou-storage-smoke-"))
    key = work / "finalizer.key"
    key.write_bytes(os.urandom(32))
    network = work / "network.bin"
    subprocess.run([node, "init-dev", str(network), str(key)], check=True)
    peers = work / "peers.txt"
    peers.write_text("".join(f"127.0.0.1 {port}\n" for port in PROVIDERS.values()))

    processes = {}
    logs = {}

    def start(name, args):
        log = open(work / f"{name}.log", "w")
        logs[name] = log
        processes[name] = subprocess.Popen(args, stdout=log, stderr=subprocess.STDOUT)

    start("finalizer", [node, "serve", str(network), str(work / "finalizer-db"), str(key),
                        "127.0.0.1", str(FINALIZER_FEED), "200", str(FINALIZER_P2P), str(peers)])
    time.sleep(2)
    for name, port in PROVIDERS.items():
        start(f"provider-{name}", [node, "provide", str(network), str(work / f"provider-{name}-db"),
                                   "127.0.0.1", str(FINALIZER_P2P), "127.0.0.1", str(port), CAPACITY])

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
            if line.startswith("HOLDER "):
                port = int(line.split()[2])
                victim = next(name for name, p in PROVIDERS.items() if p == port)
                processes[f"provider-{victim}"].kill()
                processes[f"provider-{victim}"].wait()
                print(f"[smoke] killed provider-{victim} (port {port})", flush=True)
                (client_work / "killed").touch()
        result = smoke.wait()
    finally:
        deadline.cancel()
        for process in processes.values():
            if process.poll() is None:
                process.terminate()
        for process in processes.values():
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
        for log in logs.values():
            log.close()
    if result != 0:
        for name in processes:
            print(f"----- {name} log (tail) -----")
            print("".join((work / f"{name}.log").read_text(errors="replace").splitlines(True)[-40:]))
        print("storage smoke FAILED")
        return 1
    print("storage smoke passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
