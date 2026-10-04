#!/usr/bin/env python3
"""CYBOU DEVNET battle test: a real multi-site network under load, with a report.

Topology (all ordinary Full Nodes on the current DEVNET):
  * Windows: N `cybou node run` nodes plus M `cybou-loadgen` clients (each client
    embeds its own Full Node and Identities);
  * WSL (Ubuntu): N nodes plus M clients, peering with Windows over the WSL network;
  * VPS: temporary extra nodes next to the bootstrap, on ports opened only for the run.
The Central Authority desktop must be running and finalizing: it is the PoA.

Commands:
  funder   create the funder Identity once (claims battlefunder.cybou) and show its
           balance; then send it CYBOU from the desktop (Envoyer -> battlefunder.cybou)
  run      start the topology, create and fund client Identities, run the load,
           collect metrics and write report.md in the run directory
  cleanup  stop every battle process on Windows, WSL and the VPS and close VPS ports

Test data lives under %USERPROFILE%\\cybou-battle (never in %LOCALAPPDATA%\\CYBOU),
~/cybou-battle in WSL and /home/debian/cybou-battle on the VPS.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
WIN_BIN = REPO / "build_simplified_headless" / "bin"
BASE = Path(os.environ["USERPROFILE"]) / "cybou-battle"
FUNDER_DIR = BASE / "funder"
PASSWORD_FILE = BASE / "password.txt"
GEO_SEED = Path(os.environ["LOCALAPPDATA"]) / "CYBOU" / "geo"

WSL_DISTRO = "Ubuntu-26.04"
WSL_BIN = "~/cybou/build/bin"
WSL_BASE = "~/cybou-battle"

VPS = "debian@vps-d0669a91.vps.ovh.net"
VPS_IP = "51.255.46.58"
VPS_BIN = "/home/debian/cybou/build/bin/cybou"
VPS_BASE = "/home/debian/cybou-battle"
BOOTSTRAP = (VPS_IP, 29461)

WIN_PORT, WSL_PORT, VPS_PORT = 29501, 29601, 29471
NFT_COMMENT = "cybou-battle"


def log(message: str) -> None:
    print(f"[{datetime.now():%H:%M:%S}] {message}", flush=True)


def sh(args: list[str], check: bool = True, timeout: int | None = None) -> str:
    result = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
    if check and result.returncode != 0:
        raise RuntimeError(f"{' '.join(args[:3])}... failed: {result.stderr.strip() or result.stdout.strip()}")
    return result.stdout


def wsl(script: str, check: bool = True, timeout: int | None = None) -> str:
    return sh(["wsl.exe", "-d", WSL_DISTRO, "-e", "bash", "-lc", script], check, timeout)


def vps(script: str, check: bool = True, timeout: int | None = None) -> str:
    return sh(["ssh", "-o", "BatchMode=yes", VPS, script], check, timeout)


def wsl_path(path: Path) -> str:
    """C:\\x\\y -> /mnt/c/x/y for WSL commands."""
    text = str(path).replace("\\", "/")
    return f"/mnt/{text[0].lower()}{text[2:]}"


def wsl_ip() -> str:
    return wsl("hostname -I").split()[0]


def windows_ip_from_wsl() -> str:
    return wsl("ip route | awk '/default/ {print $3}'").strip()


def ensure_password() -> None:
    BASE.mkdir(parents=True, exist_ok=True)
    if not PASSWORD_FILE.exists():
        PASSWORD_FILE.write_text(os.urandom(18).hex(), encoding="ascii")


def seed_geo(data_dir: Path) -> None:
    """Copies the desktop Geo cache so nodes start with France data (public peers)."""
    if GEO_SEED.is_dir():
        shutil.copytree(GEO_SEED, data_dir / "geo", dirs_exist_ok=True)


def peers_file(path: Path, endpoints: list[tuple[str, int]]) -> None:
    path.write_text("".join(f"{a} {p}\n" for a, p in endpoints), encoding="ascii")


# ---------------------------------------------------------------- funder

def cmd_funder(args: argparse.Namespace) -> None:
    ensure_password()
    FUNDER_DIR.mkdir(parents=True, exist_ok=True)
    seed_geo(FUNDER_DIR / "node")
    command = [str(WIN_BIN / "cybou-loadgen.exe"), "--profile", "funder", "--network", "devnet",
               "--data-dir", str(FUNDER_DIR), "--peer", f"{BOOTSTRAP[0]}:{BOOTSTRAP[1]}",
               "--password-file", str(PASSWORD_FILE), "--funder-name", args.name]
    if args.pay_accounts:
        command += ["--pay-accounts", args.pay_accounts, "--fund-each", str(args.fund_each)]
    log("funder: syncing, creating and naming the Identity (first run takes a few minutes)")
    print(sh(command), end="")


# ---------------------------------------------------------------- run

class Battle:
    def __init__(self, args: argparse.Namespace):
        self.args = args
        self.run_id = datetime.now(timezone.utc).strftime("%Y%m%d-%H%M%S")
        self.dir = BASE / self.run_id
        self.wsl_dir = f"{WSL_BASE}/{self.run_id}"
        self.vps_dir = f"{VPS_BASE}/{self.run_id}"
        self.processes: list[subprocess.Popen] = []
        self.samples: list[dict] = []

    # -- topology ------------------------------------------------------------
    def endpoints(self) -> dict[str, list[tuple[str, int]]]:
        a = self.args
        return {
            "win": [(self.win_ip, WIN_PORT + i) for i in range(a.win_nodes)],
            "wsl": [(self.wsl_ip, WSL_PORT + i) for i in range(a.wsl_nodes)],
            "vps": [(VPS_IP, VPS_PORT + i) for i in range(a.vps_nodes)],
        }

    def start_nodes(self) -> None:
        self.wsl_ip, self.win_ip = wsl_ip(), windows_ip_from_wsl()
        eps = self.endpoints()
        everyone = eps["win"] + eps["wsl"] + eps["vps"] + [BOOTSTRAP]
        capacity = self.args.capacity
        for i, (address, port) in enumerate(eps["win"]):
            node = self.dir / f"win-node-{i}"
            node.mkdir(parents=True)
            seed_geo(node)
            peers_file(node / "peers.txt", [e for e in everyone if e != (address, port)])
            self.spawn([str(WIN_BIN / "cybou.exe"), "node", "run", "--network", "devnet", "--data-dir", str(node),
                        "--listen", f"0.0.0.0:{port}", "--advertise", f"{address}:{port}",
                        "--peers", str(node / "peers.txt"), "--peer-admission", "france",
                        "--capacity", capacity, "--event-log", str(node / "events.jsonl")], node / "node.log")
        if eps["wsl"]:
            wsl(f"mkdir -p {self.wsl_dir} && cp -r '{wsl_path(GEO_SEED)}' {self.wsl_dir}/geo-seed 2>/dev/null || true")
        for i, (address, port) in enumerate(eps["wsl"]):
            node = f"{self.wsl_dir}/node-{i}"
            peers = "\\n".join(f"{a} {p}" for a, p in everyone if (a, p) != (address, port))
            wsl(f"mkdir -p {node} && cp -r {self.wsl_dir}/geo-seed {node}/geo 2>/dev/null; printf '{peers}\\n' > {node}/peers.txt; "
                f"nohup {WSL_BIN}/cybou node run --network devnet --data-dir {node} --listen 0.0.0.0:{port} "
                f"--advertise {address}:{port} --peers {node}/peers.txt --peer-admission france --capacity {capacity} "
                f"--event-log {node}/events.jsonl > {node}/node.log 2>&1 & echo $! > {node}/pid")
        if eps["vps"]:
            vps(f"sudo nft add rule inet cybou_guard input tcp dport {VPS_PORT}-{VPS_PORT + len(eps['vps']) - 1} "
                f"accept comment \\\"{NFT_COMMENT}\\\"")
        for i, (address, port) in enumerate(eps["vps"]):
            node = f"{self.vps_dir}/node-{i}"
            peers = "\\n".join(f"{a} {p}" for a, p in [("127.0.0.1", 29461)] + [e for e in eps["vps"] if e != (address, port)])
            vps(f"mkdir -p {node} && sudo cp -r /var/lib/cybou/node/state/geo {node}/geo && sudo chown -R debian {node}; "
                f"printf '{peers}\\n' > {node}/peers.txt; "
                f"sudo systemd-run --unit=cybou-battle-{i} --collect -p User=debian {VPS_BIN} node run --network devnet "
                f"--data-dir {node} --listen 0.0.0.0:{port} --advertise {address}:{port} --peers {node}/peers.txt "
                f"--peer-admission france --capacity {capacity} --event-log {node}/events.jsonl")
        log(f"nodes: windows={len(eps['win'])} wsl={len(eps['wsl'])} vps={len(eps['vps'])} (+bootstrap)")

    def spawn(self, command: list[str], log_path: Path) -> subprocess.Popen:
        out = open(log_path, "w", encoding="utf-8")
        flags = subprocess.CREATE_NEW_PROCESS_GROUP if os.name == "nt" else 0
        process = subprocess.Popen(command, stdout=out, stderr=subprocess.STDOUT, creationflags=flags)
        self.processes.append(process)
        return process

    # -- clients -------------------------------------------------------------
    def client_args(self, profile: str, duration: str, metrics: str | None) -> list[str]:
        a = self.args
        extra = ["--metrics", metrics] if metrics else []
        return ["--network", "devnet", "--identities", str(a.identities), "--profile", profile,
                "--operations-per-second", str(a.rate), "--file-size", a.file_size, "--duration", duration,
                "--replicas", str(a.replicas), "--drain-timeout", a.drain_timeout] + extra

    def win_clients(self) -> list[tuple[Path, tuple[str, int]]]:
        eps = self.endpoints()["win"] or [BOOTSTRAP]
        return [(self.dir / f"win-client-{i}", ("127.0.0.1", eps[i % len(eps)][1]) if self.endpoints()["win"] else BOOTSTRAP)
                for i in range(self.args.win_clients)]

    def wsl_clients(self) -> list[tuple[str, tuple[str, int]]]:
        eps = self.endpoints()["wsl"] or [BOOTSTRAP]
        return [(f"{self.wsl_dir}/client-{i}", ("127.0.0.1", eps[i % len(eps)][1]) if self.endpoints()["wsl"] else BOOTSTRAP)
                for i in range(self.args.wsl_clients)]

    def prepare_clients(self) -> list[str]:
        """Creates every client Identity (no load) and returns their AccountIDs."""
        log("clients: creating Identities")
        accounts: list[str] = []
        waits = []
        for path, peer in self.win_clients():
            path.mkdir(parents=True)
            for i in range(self.args.identities):
                seed_geo(path / f"identity-{i}" / "node")
            waits.append(self.spawn([str(WIN_BIN / "cybou-loadgen.exe"), "--data-dir", str(path), "--peer", f"{peer[0]}:{peer[1]}",
                                     "--password-file", str(PASSWORD_FILE)] + self.client_args("mail", "0s", None),
                                    path / "prepare.log"))
        wsl_password = f"{self.wsl_dir}/password.txt"
        if self.wsl_clients():
            wsl(f"mkdir -p {self.wsl_dir} && cp '{wsl_path(PASSWORD_FILE)}' {wsl_password} && chmod 600 {wsl_password}")
        for path, peer in self.wsl_clients():
            seeds = " ".join(f"mkdir -p {path}/identity-{i}/node && cp -r {self.wsl_dir}/geo-seed {path}/identity-{i}/node/geo 2>/dev/null;"
                             for i in range(self.args.identities))
            wsl(f"{seeds} nohup {WSL_BIN}/cybou-loadgen --data-dir {path} --peer {peer[0]}:{peer[1]} --password-file {wsl_password} "
                f"{' '.join(self.client_args('mail', '0s', None))} > {path}/prepare.log 2>&1 & echo $! > {path}/prepare.pid")
        for process in waits:
            if process.wait(timeout=900) != 0:
                raise RuntimeError("a Windows client failed to create its Identities; see prepare.log")
        for path, _ in self.wsl_clients():
            deadline = time.time() + 900
            while wsl(f"kill -0 $(cat {path}/prepare.pid) 2>/dev/null && echo running || true").strip() == "running":
                if time.time() > deadline:
                    raise RuntimeError(f"WSL client {path} did not finish preparing")
                time.sleep(5)
        for path, _ in self.win_clients():
            accounts += (path / "accounts.txt").read_text().split()
        for path, _ in self.wsl_clients():
            accounts += wsl(f"cat {path}/accounts.txt").split()
        log(f"clients: {len(accounts)} Identities ready")
        return accounts

    def fund(self, accounts: list[str]) -> None:
        if not self.args.fund_each:
            return
        listing = self.dir / "accounts.txt"
        listing.write_text("\n".join(accounts) + "\n", encoding="ascii")
        log(f"funding: {self.args.fund_each} CYBOU to each of {len(accounts)} Identities from battlefunder.cybou")
        cmd_funder(argparse.Namespace(name="battlefunder", pay_accounts=str(listing), fund_each=self.args.fund_each))

    def load(self) -> None:
        log(f"load: profile={self.args.profile} rate={self.args.rate}/s per client duration={self.args.duration}")
        processes = []
        for path, peer in self.win_clients():
            processes.append(self.spawn([str(WIN_BIN / "cybou-loadgen.exe"), "--data-dir", str(path), "--peer", f"{peer[0]}:{peer[1]}",
                                         "--password-file", str(PASSWORD_FILE)] +
                                        self.client_args(self.args.profile, self.args.duration, str(path / "metrics.json")),
                                        path / "load.log"))
        for path, peer in self.wsl_clients():
            wsl(f"nohup {WSL_BIN}/cybou-loadgen --data-dir {path} --peer {peer[0]}:{peer[1]} --password-file {self.wsl_dir}/password.txt "
                f"{' '.join(self.client_args(self.args.profile, self.args.duration, path + '/metrics.json'))} "
                f"> {path}/load.log 2>&1 & echo $! > {path}/load.pid")
        while any(p.poll() is None for p in processes) or self.wsl_load_running():
            self.sample()
            time.sleep(10)
        self.sample()

    def wsl_load_running(self) -> bool:
        return any(wsl(f"kill -0 $(cat {path}/load.pid) 2>/dev/null && echo running || true").strip() == "running"
                   for path, _ in self.wsl_clients())

    # -- measurements --------------------------------------------------------
    def sample(self) -> None:
        """CPU seconds and RSS of every battle process, per site."""
        stamp = time.time()
        try:
            win = json.loads(sh(["powershell", "-NoProfile", "-Command",
                                 "Get-Process cybou,cybou-loadgen -ErrorAction SilentlyContinue | "
                                 "Select-Object Name,Id,CPU,WorkingSet64 | ConvertTo-Json -Compress"], check=False) or "[]")
        except json.JSONDecodeError:
            win = []
        win = [win] if isinstance(win, dict) else win
        linux = "ps -C cybou,cybou-loadgen -o comm=,pid=,times=,rss= 2>/dev/null || true"
        self.samples.append({"t": stamp, "windows": win,
                             "wsl": wsl(linux, check=False).split("\n"), "vps": vps(linux, check=False).split("\n")})

    def report(self) -> None:
        metrics = []
        for path, _ in self.win_clients():
            file = path / "metrics.json"
            if file.exists():
                metrics.append(("windows:" + path.name, json.loads(file.read_text())))
        for path, _ in self.wsl_clients():
            text = wsl(f"cat {path}/metrics.json 2>/dev/null || true").strip()
            if text:
                metrics.append(("wsl:" + path.rsplit("/", 1)[1], json.loads(text)))
        (self.dir / "samples.json").write_text(json.dumps(self.samples), encoding="utf-8")
        total_ops = sum(m["operations"] for _, m in metrics)
        elapsed = max((m["elapsed_s"] for _, m in metrics), default=0)
        peak = {"windows_mib": 0.0, "wsl_mib": 0.0, "vps_mib": 0.0}
        for s in self.samples:
            peak["windows_mib"] = max(peak["windows_mib"], sum(p.get("WorkingSet64", 0) for p in s["windows"]) / 2**20)
            for site in ("wsl", "vps"):
                rss = sum(int(line.split()[-1]) for line in s[site] if line.strip())
                peak[f"{site}_mib"] = max(peak[f"{site}_mib"], rss / 1024)
        lines = [f"# CYBOU battle test {self.run_id}", "",
                 f"Topology: Windows nodes {self.args.win_nodes}, WSL nodes {self.args.wsl_nodes}, "
                 f"VPS extra nodes {self.args.vps_nodes} + bootstrap; clients Windows {self.args.win_clients}, "
                 f"WSL {self.args.wsl_clients} × {self.args.identities} Identities.",
                 f"Load: profile `{self.args.profile}`, {self.args.rate} op/s per client, {self.args.duration}, "
                 f"file size {self.args.file_size}, replicas {self.args.replicas}.", "",
                 f"**Total operations:** {total_ops} in {elapsed:.0f} s → **{(total_ops / elapsed if elapsed else 0):.2f} op/s**", "",
                 "| Client | Result | Ops | op/s | Wallet final p50/p95 ms | Publication final p50/p95 ms | Protected p50/p95 ms | Failed |",
                 "|---|---|---:|---:|---|---|---|---|"]
        for name, m in metrics:
            w, f, p = m["wallet_submit_to_final"], m["publication_submit_to_final"], m["publication_submit_to_protected"]
            lines.append(f"| {name} | {m['result']} | {m['operations']} | {m['operations_per_s']:.2f} | "
                         f"{w['p50_ms']:.0f} / {w['p95_ms']:.0f} | {f['p50_ms']:.0f} / {f['p95_ms']:.0f} | "
                         f"{p['p50_ms']:.0f} / {p['p95_ms']:.0f} | {m['failed']} |")
        lines += ["", f"Peak memory of battle processes: Windows {peak['windows_mib']:.0f} MiB, "
                      f"WSL {peak['wsl_mib']:.0f} MiB, VPS {peak['vps_mib']:.0f} MiB.",
                  "", f"Raw data: `{self.dir}` (metrics.json, events.jsonl, node.log, samples.json)."]
        (self.dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        print("\n".join(lines))

    def stop(self) -> None:
        for process in self.processes:
            if process.poll() is None:
                process.terminate()
        for process in self.processes:
            try:
                process.wait(timeout=30)
            except subprocess.TimeoutExpired:
                process.kill()
        cleanup_remote()


def cleanup_remote() -> None:
    wsl("pkill -f 'cybou-battle' || true", check=False)
    vps(f"for u in $(systemctl list-units --plain --no-legend 'cybou-battle-*' | awk '{{print $1}}'); do sudo systemctl stop $u; done; "
        f"for h in $(sudo nft -a list chain inet cybou_guard input | awk '/{NFT_COMMENT}/ {{print $NF}}'); do "
        f"sudo nft delete rule inet cybou_guard input handle $h; done", check=False)


def cmd_run(args: argparse.Namespace) -> None:
    ensure_password()
    battle = Battle(args)
    battle.dir.mkdir(parents=True)
    log(f"run {battle.run_id}: data in {battle.dir}")
    try:
        battle.start_nodes()
        time.sleep(args.warmup)
        battle.fund(battle.prepare_clients())
        battle.load()
        battle.report()
    finally:
        log("stopping battle processes")
        battle.stop()


def cmd_cleanup(_: argparse.Namespace) -> None:
    subprocess.run(["taskkill", "/F", "/T", "/IM", "cybou-loadgen.exe"], capture_output=True)
    # Only battle nodes: their command line names the battle directory, never the desktop.
    sh(["powershell", "-NoProfile", "-Command",
        "Get-CimInstance Win32_Process -Filter \"Name='cybou.exe'\" | "
        "Where-Object { $_.CommandLine -like '*cybou-battle*' } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }"], check=False)
    cleanup_remote()
    log("cleanup done")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    funder = sub.add_parser("funder")
    funder.add_argument("--name", default="battlefunder")
    funder.add_argument("--pay-accounts")
    funder.add_argument("--fund-each", type=int, default=0)
    run = sub.add_parser("run")
    run.add_argument("--win-nodes", type=int, default=3)
    run.add_argument("--wsl-nodes", type=int, default=2)
    run.add_argument("--vps-nodes", type=int, default=2)
    run.add_argument("--win-clients", type=int, default=2)
    run.add_argument("--wsl-clients", type=int, default=2)
    run.add_argument("--identities", type=int, default=2)
    run.add_argument("--profile", default="mixed", choices=["mixed", "files", "mail", "payments", "system-locks"])
    run.add_argument("--rate", type=int, default=1, help="operations per second per client")
    run.add_argument("--duration", default="10m")
    run.add_argument("--file-size", default="1MiB")
    run.add_argument("--replicas", type=int, default=2)
    run.add_argument("--capacity", default="15GiB")
    run.add_argument("--fund-each", type=int, default=100000)
    run.add_argument("--drain-timeout", default="10m")
    run.add_argument("--warmup", type=int, default=30, help="seconds for nodes to sync before clients start")
    sub.add_parser("cleanup")
    args = parser.parse_args()
    {"funder": cmd_funder, "run": cmd_run, "cleanup": cmd_cleanup}[args.command](args)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(130)
