#!/usr/bin/env python3
"""Run four real cybou-node validators over loopback and require finality."""

import argparse
import re
import socket
import subprocess
import tempfile
import time
from pathlib import Path


def free_port():
    with socket.socket() as sock:
        sock.bind(("127.0.0.1", 0))
        return sock.getsockname()[1]


def probe(binary, network, directory, port):
    result = subprocess.run(
        [str(binary), "p2p-probe", str(network), str(directory), "127.0.0.1", str(port)],
        capture_output=True,
        text=True,
        timeout=10,
        check=False,
    )
    if result.returncode:
        return None
    match = re.search(r"\bheight=(\d+)\b", result.stdout)
    return int(match.group(1)) if match else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to cybou-node executable")
    parser.add_argument("--timeout", type=int, default=90)
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"missing executable: {binary}")

    with tempfile.TemporaryDirectory(prefix="cybou-four-process-") as temp:
        root = Path(temp)
        network = root / "network.bin"
        keys = []
        for index in range(4):
            key = root / f"validator-{index}.key"
            key.write_bytes(bytes([0xA1 + index]) * 32)
            keys.append(key)
        subprocess.run([str(binary), "init-dev", str(network), *(str(key) for key in keys)],
                       check=True, capture_output=True, text=True, timeout=30)

        used_ports = set()
        ports = []
        for _ in range(8):
            port = free_port()
            while port in used_ports:
                port = free_port()
            used_ports.add(port)
            ports.append(port)
        feed_ports, p2p_ports = ports[:4], ports[4:]

        peers = []
        for index in range(4):
            peer_file = root / f"peers-{index}.txt"
            peer_file.write_text("".join(
                f"127.0.0.1 {p2p_ports[other]}\n" for other in range(4) if other != index
            ), encoding="ascii")
            peers.append(peer_file)

        processes = []
        logs = []
        def start_validator(index):
            log = (root / f"node-{index}.log").open("a", encoding="utf-8")
            logs.append(log)
            return subprocess.Popen([
                str(binary), "serve", str(network), str(root / f"db-{index}"),
                str(keys[index]), "127.0.0.1", str(feed_ports[index]), "250",
                str(p2p_ports[index]), str(peers[index]),
            ], stdout=log, stderr=subprocess.STDOUT)

        def wait_for(target, active):
            deadline = time.monotonic() + args.timeout
            heights = [None] * 4
            while time.monotonic() < deadline:
                for index in active:
                    process = processes[index]
                    if process.poll() is not None:
                        raise RuntimeError(f"validator {index} exited with {process.returncode}")
                    heights[index] = probe(binary, network, root / f"probe-{index}", p2p_ports[index])
                if all(heights[index] is not None and heights[index] >= target for index in active):
                    return heights
                time.sleep(0.5)
            raise TimeoutError(f"validators {active} did not reach height {target}: {heights}")

        try:
            for index in range(4):
                processes.append(start_validator(index))

            heights = wait_for(2, range(4))
            offline = (max(heights) + 1) % 4
            processes[offline].terminate()
            processes[offline].wait(timeout=5)
            active = [index for index in range(4) if index != offline]
            target = max(heights) + 2
            degraded = wait_for(target, active)
            processes[offline] = start_validator(offline)
            recovered = wait_for(target, range(4))
            print(f"four-process finality: {heights}; offline validator {offline}: "
                  f"{degraded}; recovered: {recovered}")
        except Exception:
            for log in logs:
                log.flush()
            for index in range(4):
                print(f"node {index}:\n{(root / f'node-{index}.log').read_text(encoding='utf-8')[-3000:]}")
            raise
        finally:
            for process in processes:
                if process.poll() is None:
                    process.terminate()
            for process in processes:
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
            for log in logs:
                log.close()


if __name__ == "__main__":
    main()
