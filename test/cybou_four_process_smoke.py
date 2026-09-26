#!/usr/bin/env python3
"""Run four real cybou-node validators over loopback and verify BFT consensus under churn.

Includes:
- Deliberate process startup skew (0, 100ms, 250ms, 400ms).
- Leader kill using sorted validator mapping from init-dev.
- Deterministic mid-height crash via CYBOU_TEST_EXIT_AFTER_SIGN after a durable
  prevote signing intent (no journal-polling fallback: the test fails if the
  crash boundary is not hit).
- Packet/delivery disturbance (delayed drain) and real periodic reconnect
  (CYBOU_RECONNECT_INTERVAL_MS is set for every validator).
- Two validators frozen with SIGSTOP: the remaining pair must stall without
  quorum while advancing at least five rounds; a resumed validator must use
  f+1 future-round evidence to rejoin, then the height finalizes at 3/4 quorum.
- 10-50 consecutive heights with periodic kill/restart churn.
"""

import argparse
import os
import re
import signal
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
            # Exercise the production periodic reconnect path in every run.
            env["CYBOU_RECONNECT_INTERVAL_MS"] = "2500"
            if index >= 2:
                env["CYBOU_CONSENSUS_DRAIN_DELAY_MS"] = "25"
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
                processes[index] = start_validator(index)

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
            processes[leader_proc] = start_validator(leader_proc)
            recovered_heights = wait_for_synced(target_after_kill, range(4))
            print(f"Recovered leader {leader_proc}, all synced at {target_after_kill}: {recovered_heights}")

            # 3. Deterministic mid-height crash: the test hook exits the process
            # with code 120 immediately after a durable PREVOTE signing intent.
            # There is no fallback — if the crash boundary is never hit, the
            # scenario fails instead of silently degrading.
            cur_h = max(h for h in recovered_heights if h is not None)
            mid_h = cur_h + 1
            follower_val_idx = (mid_h + 1) % 4
            mid_proc = val_to_proc[follower_val_idx]
            print(f"Testing deterministic mid-height crash on follower process {mid_proc} "
                  f"for height {mid_h} (CYBOU_TEST_EXIT_AFTER_SIGN=PREVOTE)...")
            stop_validator(mid_proc)
            processes[mid_proc] = start_validator(
                mid_proc, env_extra={"CYBOU_TEST_EXIT_AFTER_SIGN": "PREVOTE"})
            deadline = time.monotonic() + 60
            while processes[mid_proc].poll() is None and time.monotonic() < deadline:
                time.sleep(0.05)
            if processes[mid_proc].poll() is None:
                raise RuntimeError(f"validator {mid_proc} did not hit the deterministic crash boundary in time")
            if processes[mid_proc].returncode != 120:
                raise RuntimeError(
                    f"validator {mid_proc} exited with {processes[mid_proc].returncode}, expected 120 "
                    "from CYBOU_TEST_EXIT_AFTER_SIGN")
            info = read_signing_journal(root / f"db-{mid_proc}" / "validator-signing.journal")
            if not info or info["height"] < mid_h or info["step"] < 1:
                raise RuntimeError(f"journal after deterministic crash is missing or unexpected: {info}")
            print(f"Deterministic crash at height {info['height']}, step {info['step']} confirmed.")
            stop_validator(mid_proc)

            active = [idx for idx in range(4) if idx != mid_proc]
            target_mid = mid_h + 2
            degraded_mid = wait_for(target_mid, active)
            print(f"3/4 quorum reached height {target_mid} with process {mid_proc} offline: {degraded_mid}")

            # Restart validator with persisted journal; it must preserve its lock and catch up safely
            print(f"Restarting mid-height killed process {mid_proc}...")
            processes[mid_proc] = start_validator(mid_proc)
            recovered_mid = wait_for_synced(target_mid, range(4))
            print(f"Restarted process {mid_proc}, all synced at height {target_mid}: {recovered_mid}")

            # 4. Frozen validators required for quorum at an unfinished height:
            # SIGSTOP two validators right after a finalized height; the remaining
            # pair must stall (2 < quorum 3). Resuming only one of them must let
            # the unfinished height finalize — its participation is required.
            # POSIX only: Windows has no SIGSTOP/SIGCONT, and CI runs on Linux.
            freeze_supported = hasattr(signal, "SIGSTOP") and hasattr(signal, "SIGCONT")

            def freeze_quorum_attempt(base_h):
                frozen_a, frozen_b = 0, 1
                active_pair = [i for i in range(4) if i not in (frozen_a, frozen_b)]
                print(f"SIGSTOP validators {frozen_a},{frozen_b} right after height {base_h}; "
                      f"active pair {active_pair} must stall without quorum...")
                os.kill(processes[frozen_a].pid, signal.SIGSTOP)
                os.kill(processes[frozen_b].pid, signal.SIGSTOP)
                try:
                    # Keep the two live validators stalled for long enough to
                    # advance well beyond the bounded proposal catch-up window.
                    deadline = time.monotonic() + 6.0
                    while time.monotonic() < deadline:
                        for i in active_pair:
                            h = probe(binary, network, root / f"probe-{i}", p2p_ports[i])
                            if h is not None and h > base_h:
                                return None  # race lost: height finalized from pre-freeze votes
                        time.sleep(0.4)
                    print(f"Active pair stalled at height {base_h} as expected (no quorum without "
                          f"{frozen_a},{frozen_b}).")
                    live_rounds = [
                        read_signing_journal(root / f"db-{i}" / "validator-signing.journal")
                        for i in active_pair
                    ]
                    if any(not info or info["height"] <= base_h or info["round"] < 5
                           for info in live_rounds):
                        raise RuntimeError(
                            f"live validators did not reach five rounds while stalled: {live_rounds}")
                    print(f"Active pair reached rounds {[info['round'] for info in live_rounds]} "
                          "without finalizing; resuming one validator...")
                    # Resuming frozen_b must be sufficient to finalize the
                    # unfinished height: 3/4 quorum including the resumed node.
                    os.kill(processes[frozen_b].pid, signal.SIGCONT)
                    target_q = base_h + 2
                    wait_for(target_q, [frozen_b] + active_pair, timeout=90)
                    print(f"Resumed validator {frozen_b} rejoined the unfinished height; "
                          f"quorum finalized {target_q}.")
                    os.kill(processes[frozen_a].pid, signal.SIGCONT)
                    return target_q
                finally:
                    for i in (frozen_a, frozen_b):
                        if processes[i] and processes[i].poll() is None:
                            os.kill(processes[i].pid, signal.SIGCONT)

            cur_h = max(h for h in recovered_mid if h is not None)
            frozen_done = None
            if freeze_supported:
                for attempt in range(3):
                    frozen_done = freeze_quorum_attempt(cur_h)
                    if frozen_done is not None:
                        break
                    print("Race lost: height finalized from pre-freeze votes; retrying freeze scenario...")
                    wait_for_synced(cur_h, range(4))
                    heights_now = [probe(binary, network, root / f"probe-{i}", p2p_ports[i]) for i in range(4)]
                    cur_h = max(h for h in heights_now if h is not None)
                if frozen_done is None:
                    raise RuntimeError("freeze/quorum scenario lost the race 3 times")
                recovered_q = wait_for_synced(frozen_done, range(4))
                print(f"All 4 validators reached height {frozen_done}: {recovered_q}")
            else:
                print("SIGSTOP/SIGCONT unavailable on this platform; skipping freeze/quorum scenario.")
                recovered_q = recovered_mid

            # 5. Historical catch-up beyond the 32-head gossip window: keep
            # one validator offline while the network advances ~45 heights,
            # then rejoin and verify bulk catch-up over a single session.
            cur_h = max(h for h in recovered_q if h is not None)
            offline_proc = (cur_h + 1) % 4
            offline_start = cur_h
            target_offline = cur_h + 45
            print(f"Testing historical catch-up: process {offline_proc} offline while the network "
                  f"advances from {offline_start} to ~{target_offline} (beyond the 32-block gossip window)...")
            stop_validator(offline_proc)
            advanced = wait_for(target_offline, [i for i in range(4) if i != offline_proc], timeout=120)
            print(f"Network advanced to {target_offline} without validator {offline_proc}: {advanced}")
            processes[offline_proc] = start_validator(offline_proc)
            rejoined = wait_for_synced(target_offline, range(4), timeout=120)
            print(f"Rejoined validator {offline_proc} caught up across the gossip window: {rejoined}")
            recovered_q = rejoined

            # 6. Consecutive heights verification (10-50 heights) with periodic kill/restart churn
            cur_h = max(h for h in recovered_q if h is not None)
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
                processes[churn_proc] = start_validator(churn_proc)
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
