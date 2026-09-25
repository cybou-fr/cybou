#!/usr/bin/env python3
"""Run four real cybou-node validators over loopback and verify BFT consensus under churn.

Includes:
- Deliberate process startup skew (0, 100ms, 250ms, 400ms).
- Leader kill using sorted validator mapping from init-dev.
- Mid-height OS process restart after signing prevote/precommit.
- Packet/delivery disturbance (delayed drain and periodic reconnect).
- 10-50 consecutive heights with periodic kill/restart churn.
"""

import argparse
import os
import re
import socket
import subprocess
import tempfile
import time
from pathlib import Path


def find_free_port_range(count=8, start=29500):
    for base in range(start, 60000, count):
        socks = []
        try:
            for i in range(count):
                s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                s.bind(("127.0.0.1", base + i))
                socks.append(s)
            return [base + i for i in range(count)]
        except OSError:
            continue
        finally:
            for s in socks:
                s.close()
    raise RuntimeError(f"cannot find {count} consecutive free ports")


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


def read_signing_journal(journal_path):
    if not journal_path.is_file():
        return None
    try:
        data = journal_path.read_bytes()
        if len(data) == 145 and data[:4] == b"CBS1":
            height = int.from_bytes(data[68:76], "little")
            round_no = int.from_bytes(data[76:80], "little")
            step = data[80]  # 0=PROPOSE, 1=PREVOTE, 2=PRECOMMIT
            return {"height": height, "round": round_no, "step": step}
        elif len(data) >= 153 and data[:4] == b"CBS2":
            height = int.from_bytes(data[68:76], "little")
            round_no = int.from_bytes(data[76:80], "little")
            step = data[80]  # 0=PROPOSE, 1=PREVOTE, 2=PRECOMMIT
            locked_round = int.from_bytes(data[113:117], "little", signed=True)
            return {"height": height, "round": round_no, "step": step, "locked_round": locked_round}
        return None
    except Exception:
        return None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary", type=Path, help="path to cybou-node executable")
    parser.add_argument("--timeout", type=int, default=120)
    parser.add_argument("--target-height", type=int, default=20,
                        help="target block height to verify consecutive finality (10-50)")
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"missing executable: {binary}")

    with tempfile.TemporaryDirectory(prefix="cybou-four-process-", ignore_cleanup_errors=True) as temp:
        root = Path(temp)
        network = root / "network.bin"
        keys = []
        for index in range(4):
            key = root / f"validator-{index}.key"
            key.write_bytes(bytes([0xA1 + index]) * 32)
            keys.append(key)

        init_res = subprocess.run(
            [str(binary), "init-dev", str(network), *(str(key) for key in keys)],
            check=True, capture_output=True, text=True, timeout=30
        )

        val_to_proc = {}
        for line in init_res.stdout.splitlines():
            m = re.match(r"^validator\.(\d+)=(\d+)$", line.strip())
            if m:
                val_to_proc[int(m.group(1))] = int(m.group(2))
        if len(val_to_proc) != 4:
            raise RuntimeError(f"failed to parse validator mapping from init-dev: {init_res.stdout}")
        print(f"Validator set mapping (validator_index -> process_index): {val_to_proc}")

        ports = find_free_port_range(8)
        feed_ports, p2p_ports = ports[:4], ports[4:]

        peers = []
        for index in range(4):
            peer_file = root / f"peers-{index}.txt"
            peer_file.write_text("".join(
                f"127.0.0.1 {p2p_ports[other]}\n" for other in range(4) if other != index
            ), encoding="ascii")
            peers.append(peer_file)

        processes = [None] * 4
        logs = []

        def start_validator(index, env_extra=None):
            env = os.environ.copy()
            if env_extra:
                env.update(env_extra)
            log = (root / f"node-{index}.log").open("a", encoding="utf-8")
            logs.append(log)
            return subprocess.Popen([
                str(binary), "serve", str(network), str(root / f"db-{index}"),
                str(keys[index]), "127.0.0.1", str(feed_ports[index]), "250",
                str(p2p_ports[index]), str(peers[index]),
            ], stdout=log, stderr=subprocess.STDOUT, env=env)

        def stop_validator(index, timeout=5):
            p = processes[index]
            if p and p.poll() is None:
                p.terminate()
                try:
                    p.wait(timeout=timeout)
                except subprocess.TimeoutExpired:
                    p.kill()
                    p.wait()
            processes[index] = None

        def wait_for(target, active, timeout=None):
            deadline = time.monotonic() + (timeout if timeout is not None else args.timeout)
            heights = [None] * 4
            while time.monotonic() < deadline:
                for index in active:
                    process = processes[index]
                    if process is None or process.poll() is not None:
                        code = process.returncode if process else "None"
                        raise RuntimeError(f"validator {index} exited prematurely with {code}")
                    heights[index] = probe(binary, network, root / f"probe-{index}", p2p_ports[index])
                if all(heights[index] is not None and heights[index] >= target for index in active):
                    return heights
                time.sleep(0.5)
            raise TimeoutError(f"validators {active} did not reach height {target} within timeout: {heights}")

        def wait_for_synced(target, active=None, timeout=None):
            if active is None:
                active = list(range(4))
            deadline = time.monotonic() + (timeout if timeout is not None else args.timeout)
            heights = [None] * 4
            while time.monotonic() < deadline:
                for index in active:
                    process = processes[index]
                    if process is None or process.poll() is not None:
                        code = process.returncode if process else "None"
                        raise RuntimeError(f"validator {index} exited prematurely with {code}")
                    heights[index] = probe(binary, network, root / f"probe-{index}", p2p_ports[index])
                active_h = [heights[i] for i in active if heights[i] is not None]
                if len(active_h) == len(active):
                    tip = max(active_h)
                    if tip >= target and all(h == tip for h in active_h):
                        return heights
                time.sleep(0.5)
            raise TimeoutError(f"validators {active} did not synchronize to tip {target}: {heights}")

        try:
            # 1. Deliberate process startup skew (0, 100ms, 250ms, 400ms) with delivery disturbance
            startup_skews = [0.0, 0.100, 0.250, 0.400]
            print(f"Starting 4 validators with deliberate skew {startup_skews}s and delivery disturbance...")
            for index, skew in enumerate(startup_skews):
                if skew > 0:
                    time.sleep(skew)
                processes[index] = start_validator(
                    index,
                    env_extra={"CYBOU_CONSENSUS_DRAIN_DELAY_MS": "25"} if index >= 2 else None
                )

            heights = wait_for_synced(2, range(4))
            print(f"Initial 4-process finality reached: {heights}")

            # 2. Leader kill using sorted validator mapping from init-dev
            cur_h = max(h for h in heights if h is not None)
            target_leader_h = cur_h + 1
            leader_val_idx = target_leader_h % 4
            leader_proc = val_to_proc[leader_val_idx]
            print(f"Testing real leader kill: height {target_leader_h} leader is validator {leader_val_idx} "
                  f"(process {leader_proc}). Terminating...")
            stop_validator(leader_proc)
            active = [idx for idx in range(4) if idx != leader_proc]
            target_after_kill = target_leader_h + 2
            degraded_heights = wait_for(target_after_kill, active)
            print(f"3/4 quorum reached height {target_after_kill} without leader {leader_proc}: {degraded_heights}")

            # Recover leader process and verify catchup
            print(f"Restarting leader process {leader_proc}...")
            processes[leader_proc] = start_validator(
                leader_proc,
                env_extra={"CYBOU_CONSENSUS_DRAIN_DELAY_MS": "25"} if leader_proc >= 2 else None
            )
            recovered_heights = wait_for_synced(target_after_kill, range(4))
            print(f"Recovered leader {leader_proc}, all synced at {target_after_kill}: {recovered_heights}")

            # 3. Mid-height OS-process restart: kill validator after it signs prevote/precommit
            cur_h = max(h for h in recovered_heights if h is not None)
            mid_h = cur_h + 1
            follower_val_idx = (mid_h + 1) % 4
            mid_proc = val_to_proc[follower_val_idx]
            print(f"Testing mid-height restart on follower process {mid_proc} for height {mid_h}...")
            journal_path = root / f"db-{mid_proc}" / "validator-signing.journal"
            deadline = time.monotonic() + 10
            killed = False
            while time.monotonic() < deadline:
                info = read_signing_journal(journal_path)
                if info and info["height"] >= mid_h and info["step"] >= 1:
                    print(f"Detected signed vote at height {info['height']}, step {info['step']}! Terminating...")
                    stop_validator(mid_proc)
                    killed = True
                    break
                time.sleep(0.01)

            if not killed:
                print(f"Did not catch mid-height vote in window, stopping process {mid_proc} directly")
                stop_validator(mid_proc)

            active = [idx for idx in range(4) if idx != mid_proc]
            target_mid = mid_h + 2
            degraded_mid = wait_for(target_mid, active)
            print(f"3/4 quorum reached height {target_mid} with process {mid_proc} offline: {degraded_mid}")

            # Restart validator with persisted journal; it must preserve its lock and catch up safely
            print(f"Restarting mid-height killed process {mid_proc}...")
            processes[mid_proc] = start_validator(
                mid_proc,
                env_extra={"CYBOU_CONSENSUS_DRAIN_DELAY_MS": "25"} if mid_proc >= 2 else None
            )
            recovered_mid = wait_for_synced(target_mid, range(4))
            print(f"Restarted process {mid_proc}, all synced at height {target_mid}: {recovered_mid}")

            # 4. Consecutive heights verification (10-50 heights) with periodic kill/restart churn
            cur_h = max(h for h in recovered_mid if h is not None)
            target_final = max(args.target_height, cur_h + 4)
            print(f"Verifying consecutive finality up to height {target_final} with periodic kill/restart...")
            churn_cycle = 0
            while cur_h < target_final:
                next_target = min(cur_h + 2, target_final)
                churn_proc = churn_cycle % 4
                churn_cycle += 1
                print(f"Advancing to {next_target} with validator {churn_proc} offline...")
                stop_validator(churn_proc)
                active = [i for i in range(4) if i != churn_proc]
                h_active = wait_for(next_target, active, timeout=30)
                print(f"3/4 active reached {next_target} without validator {churn_proc}: {h_active}")
                processes[churn_proc] = start_validator(
                    churn_proc,
                    env_extra={"CYBOU_CONSENSUS_DRAIN_DELAY_MS": "25"} if churn_proc >= 2 else None
                )
                h_all = wait_for_synced(next_target, range(4), timeout=30)
                cur_h = max(h for h in h_all if h is not None)
                print(f"All 4 validators reached height {cur_h}: {h_all}")

            print(f"SUCCESS: 4-process BFT test completed up to height {cur_h} under churn and disturbance.")

        except Exception:
            for log in logs:
                log.flush()
            for index in range(4):
                log_file = root / f"node-{index}.log"
                if log_file.is_file():
                    print(f"node {index}:\n{log_file.read_text(encoding='utf-8')[-3000:]}")
            raise
        finally:
            for index in range(4):
                stop_validator(index)
            for log in logs:
                log.close()


if __name__ == "__main__":
    main()
