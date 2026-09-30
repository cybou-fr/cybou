#!/usr/bin/env python3
"""CYBOU LAB controller. CLI/SSH/WSL process orchestration; no daemon API.

The worker mode runs this same file on each host. Only public event logs are
collected. Destructive actions require an owned LAB root and verified PID birth
identity; DEV ports and paths are forbidden.
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import shutil
import signal
import subprocess
import sys
import time
import tomllib
import uuid
import cybou_lab_network as lab_network

DEV_PORTS = {29461, 29471, 29481}


def inside(root, path):
    root, path = Path(root).resolve(), Path(path).resolve()
    if path == root or root not in path.parents:
        raise ValueError("path must be strictly inside owned LAB root")
    return path


def birth(pid):
    """PID reuse protection, without a third-party process library."""
    if os.name == "nt":
        import ctypes
        from ctypes import wintypes
        k = ctypes.WinDLL("kernel32", use_last_error=True)
        k.OpenProcess.restype = wintypes.HANDLE
        handle = k.OpenProcess(0x1000, False, int(pid))
        if not handle:
            return None
        try:
            code = wintypes.DWORD()
            k.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
            if not k.GetExitCodeProcess(handle, ctypes.byref(code)) or code.value != 259:
                return None
            values = [wintypes.FILETIME() for _ in range(4)]
            k.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4
            if not k.GetProcessTimes(handle, *[ctypes.byref(v) for v in values]):
                return None
            return str(values[0].dwHighDateTime << 32 | values[0].dwLowDateTime)
        finally:
            k.CloseHandle.argtypes = [wintypes.HANDLE]
            k.CloseHandle(handle)
    try:
        stat = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return None if stat[0] == "Z" else stat[19]
    except (FileNotFoundError, ProcessLookupError):
        return None


def worker(req):
    root = Path(req["root"]).resolve()
    if "cybou-lab" not in root.name or root.name in {"cybou", "dev"}:
        raise ValueError("worker root name must contain cybou-lab")
    marker = root / ".cybou-lab-owner"
    action = req["action"]
    if action == "prepare":
        root.mkdir(parents=True, exist_ok=True)
        if marker.exists() and marker.read_text() != req["token"]:
            raise ValueError("LAB belongs to another run")
        if not marker.exists():
            if any(root.iterdir()):
                raise ValueError("refusing to adopt non-empty directory")
            marker.write_text(req["token"])
        if req.get("namespace"):
            lab_network.namespace(root,req["namespace"],prepare=True)
        return {"prepared": True}
    if not marker.exists() or marker.read_text() != req["token"]:
        raise ValueError("missing LAB ownership marker")
    if action in {"network_fault","network_reset","namespace_cleanup"}:
        name=req.get("namespace")
        if not name: raise ValueError("network faults require an owned isolated namespace")
        if action == "namespace_cleanup": return lab_network.cleanup(root,name)
        return lab_network.fault(root,name,int(req["port"]),int(req.get("delay_ms",0)),int(req.get("loss_percent",0)),action=="network_reset",req.get("all_tcp",False))
    if action == "put_network":
        path = inside(root, root / "network.bin")
        content = bytes.fromhex(req["hex"])
        if path.exists() and path.read_bytes() != content:
            raise ValueError("refusing LAB network replacement")
        path.write_bytes(content)
        return {"ok": True}
    if action == "init":
        key = inside(root, root / "finalizer.seed")
        network = inside(root, root / "network.bin")
        if not network.exists():
            if not key.exists():
                descriptor = os.open(key, os.O_CREAT | os.O_EXCL | os.O_WRONLY, 0o600)
                with os.fdopen(descriptor, "wb") as stream:
                    stream.write(os.urandom(32))
            subprocess.run([req["binary"], "network", "init-dev", "--network", str(network), "--key-file", str(key)], check=True, stdout=subprocess.DEVNULL)
        public = subprocess.check_output([req["binary"], "network", "info", "--network", str(network)], text=True)
        return {"network_hex": network.read_bytes().hex(), "info": public}
    if action == "prepare_password":
        path = inside(root, root / "loadgen.password")
        if not path.exists():
            fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
            with os.fdopen(fd, "w") as stream:
                stream.write(os.urandom(32).hex())
        return {"ok": True}
    if action == "write_peers":
        peers = req["peers"]
        if any(int(port) in DEV_PORTS or int(port) < 30000 for address, port in peers):
            raise ValueError("DEV/reserved ports forbidden")
        name = req["name"]
        if not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}", name):
            raise ValueError("invalid peer file name")
        (root / f"{name}.peers.txt").write_text("".join(f"{address} {port}\n" for address, port in peers))
        return {"ok": True}
    name = req.get("name", "")
    if not re.fullmatch(r"[a-zA-Z0-9_-]{1,64}", name):
        raise ValueError("invalid LAB node name")
    state = inside(root, root / f"{name}.process.json")
    stored = json.loads(state.read_text()) if state.exists() else None
    alive = bool(stored and birth(stored["pid"]) == stored["birth"])
    if action == "status":
        log = root / f"{name}.stdout.log"
        last = ""
        if log.exists():
            with log.open("rb") as stream:
                stream.seek(max(0, log.stat().st_size - 1024))
                lines = stream.read().decode(errors="replace").splitlines()
                last = lines[-1] if lines else ""
        metrics = {"disk_free": shutil.disk_usage(root).free}
        if alive and os.name != "nt" and Path("/proc").exists():
            pid = stored["pid"]
            try:
                fields = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
                metrics.update(cpu_ticks=int(fields[11])+int(fields[12]),
                    rss_bytes=int(fields[21])*os.sysconf("SC_PAGE_SIZE"),
                    open_fds=len(list(Path(f"/proc/{pid}/fd").iterdir())))
            except FileNotFoundError:
                alive = False
        return {"alive": alive, "pid": stored["pid"] if alive else None, "last_line": last, "metrics": metrics}
    if action == "events":
        file = inside(root, root / req.get("relative_log", f"{name}.events.jsonl"))
        offset = int(req.get("offset", 0))
        if not file.exists():
            return {"offset": offset, "events": []}
        if file.stat().st_size < offset:
            raise ValueError("append-only event log truncated")
        if file.stat().st_size > 1 << 30:
            raise ValueError("event log 1GiB ceiling exceeded; preserve and start a fresh LAB directory")
        with file.open("rb") as stream:
            stream.seek(offset)
            data = stream.read(4 << 20)
        data = data[: data.rfind(b"\n") + 1]
        return {"offset": offset + len(data), "events": [json.loads(line) for line in data.splitlines()]}
    if action in {"stop", "kill"}:
        if alive:
            if os.name == "nt":
                import ctypes
                delivered = False
                if action == "stop":
                    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
                    kernel.FreeConsole()
                    if kernel.AttachConsole(int(stored["pid"])):
                        delivered = bool(kernel.GenerateConsoleCtrlEvent(1, int(stored["pid"])))
                        kernel.FreeConsole()
                if not delivered:
                    subprocess.run(["taskkill", "/PID", str(stored["pid"]), "/T", "/F"], check=True, capture_output=True)
            else:
                os.kill(stored["pid"], signal.SIGKILL if action == "kill" else signal.SIGTERM)
            deadline = time.monotonic() + 20
            while birth(stored["pid"]) == stored["birth"] and time.monotonic() < deadline:
                time.sleep(.1)
            if birth(stored["pid"]) == stored["birth"]:
                raise RuntimeError("process did not stop; force kill must be explicit")
        return {"alive": False}
    if action == "corrupt":
        chunk = req["chunk_id"]
        if not re.fullmatch("[0-9a-f]{64}", chunk):
            raise ValueError("invalid ChunkID")
        path = inside(root, root / f"{name}.db.chunks" / "chunks" / chunk[:2] / chunk[2:4] / chunk)
        if path.is_symlink() or path.stat().st_size == 0:
            raise ValueError("invalid LAB blob")
        with path.open("r+b") as stream:
            stream.seek(path.stat().st_size // 2)
            old = stream.read(1)
            stream.seek(-1, 1)
            stream.write(bytes([old[0] ^ 1]))
        return {"corrupted": chunk}
    if action in {"doctor", "start"}:
        args = req["args"]
        # Validate all mutable outputs, even for user-supplied manifest arguments.
        for option in ("--data-dir", "--event-log", "--key-file", "--peers", "--password-file"):
            if option in args:
                inside(root, args[args.index(option) + 1])
        for option in ("--peer", "--listen"):
            if option in args and int(args[args.index(option) + 1].rsplit(":", 1)[1]) < 30000:
                raise ValueError("DEV/reserved port forbidden")
        if req.get("namespace"):
            args=lab_network.namespace(root,req["namespace"])+args
        if action == "doctor":
            result = subprocess.run(args, capture_output=True, text=True, timeout=180)
            return {"code": result.returncode, "output": result.stdout + result.stderr}
        if alive:
            if stored["args_hash"] != hashlib.sha256(json.dumps(args).encode()).hexdigest():
                raise ValueError("running LAB process arguments differ")
            return {"pid": stored["pid"], "already_running": True}
        log = inside(root, root / f"{name}.stdout.log")
        startup = None
        if os.name == "nt":
            startup = subprocess.STARTUPINFO()
            startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
            startup.wShowWindow = 0
        with log.open("ab") as stream:
            process = subprocess.Popen(args, stdin=subprocess.DEVNULL, stdout=stream, stderr=stream,
                startupinfo=startup, start_new_session=os.name != "nt",
                creationflags=(subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NEW_CONSOLE) if os.name == "nt" else 0)
        identity = birth(process.pid)
        if identity is None:
            raise RuntimeError("LAB process failed to start")
        temp = state.with_suffix(".tmp")
        temp.write_text(json.dumps({"pid": process.pid, "birth": identity, "args_hash": hashlib.sha256(json.dumps(args).encode()).hexdigest()}))
        temp.replace(state)
        return {"pid": process.pid}
    raise ValueError("unknown worker action")


class Lab:
    def __init__(self, manifest):
        self.path = Path(manifest).resolve()
        self.config = tomllib.loads(self.path.read_text())
        self.hosts = self.config["hosts"]
        self.nodes = self.config["nodes"]
        self.dir = Path(self.config["lab"].get("run_dir", str(self.path.parent / "runs" / self.config["lab"]["name"]))).resolve()
        self.dir.mkdir(parents=True, exist_ok=True)
        tokenfile = self.dir / "owner.token"
        if not tokenfile.exists():
            tokenfile.write_text(str(uuid.uuid4()))
        self.token = tokenfile.read_text()
        self.offsets = {}
        self.latest = {}
        self.sequence = {}
        self.heads = {}
        self.node_heads = {}
        self.convergence_target = None
        self.finalized = {}
        self.nonces = {}
        self.accepted = {}
        self.latencies = []
        self.transitions = {}
        self.transition_latencies = {"protection": [], "repair": [], "reconnect": []}
        self.failures = []
        self.loadgen = self.config.get("loadgen")
        self.loadgen_passed = False
        self.network_id = (self.dir / "network.id").read_text() if (self.dir / "network.id").exists() else None
        names = [node["name"] for node in self.nodes]
        if len(set(names)) != len(names) or len(names) > 128:
            raise ValueError("duplicate names or too many nodes")
        if sum(node["role"] == "finalizer" for node in self.nodes) != 1:
            raise ValueError("LAB requires exactly one finalizer")
        for node in self.nodes:
            if node["role"] not in {"finalizer", "provider", "observer"}:
                raise ValueError("invalid role")
            for field in ("listen", "peer", "advertise"):
                if field in node and int(node[field].rsplit(":", 1)[1]) < 30000:
                    raise ValueError("LAB ports must be >=30000")
        timeline = self.dir / "timeline.jsonl"
        self.evidence_invalid = False
        if timeline.exists():
            with timeline.open() as source:
                for line in source:
                    event = json.loads(line)
                    try:
                        self.ingest(event.pop("node"), event)
                    except RuntimeError:
                        self.evidence_invalid = True
                        break
        checkpoint = self.dir / "offsets.json"
        if checkpoint.exists():
            self.offsets = json.loads(checkpoint.read_text())
        failure = self.dir / "failure.json"
        if failure.exists():
            self.failures.extend(json.loads(failure.read_text()))

    def call(self, host, action, **fields):
        spec = self.hosts[host]
        payload = {"action": action, "root": spec["root"], "token": self.token, "namespace":spec.get("namespace"), **fields}
        python = spec.get("python", "python3")
        script = spec.get("worker", str(Path(__file__).resolve()))
        command = [python, script, "--worker"]
        if spec.get("sudo_worker"):
            command=["sudo","-n",*command]
        if spec["type"] == "ssh":
            command = ["ssh", "-o", "BatchMode=yes", spec["target"], shlex.join(command)]
        elif spec["type"] == "wsl":
            command = ["wsl", "-d", spec["distro"], *(["-u",spec["user"]] if spec.get("user") else []), "--", *command]
        elif spec["type"] not in {"local", "windows"}:
            raise ValueError("unknown host transport")
        result = subprocess.run(command, input=json.dumps(payload), text=True, capture_output=True, timeout=240)
        if result.returncode:
            raise RuntimeError(result.stderr.strip() or result.stdout.strip())
        return json.loads(result.stdout)

    def args(self, node, doctor=False):
        host = self.hosts[node["host"]]
        root = host["root"].rstrip("/\\")
        sep = "\\" if host["type"] == "windows" else "/"
        join = lambda name: root + sep + name
        args = [host["binary"], "doctor"] if doctor else [host["binary"], node["role"], "run"]
        args += ["--network", join("network.bin"), "--data-dir", join(node["name"] + ".db")]
        if "listen" in node:
            args += ["--listen", node["listen"]]
        if not doctor and "advertise" in node:
            args += ["--advertise", node["advertise"]]
        if node["role"] == "finalizer":
            args += ["--key-file", join("finalizer.seed")]
            if not doctor:
                args += ["--block-interval", node.get("block_interval", "500ms")]
        elif not doctor:
            args += ["--peer", node["peer"]]
            if node["role"] == "provider":
                args += ["--capacity", node.get("capacity", "20GiB")]
        args += ["--peers", join(node["name"] + ".peers.txt")]
        if not doctor:
            args += ["--event-log", join(node["name"] + ".events.jsonl")]
        return args

    def init(self):
        for host in self.hosts:
            self.call(host, "prepare")
        finalizer = next(node for node in self.nodes if node["role"] == "finalizer")
        network = self.call(finalizer["host"], "init", binary=self.hosts[finalizer["host"]]["binary"])
        for host in self.hosts:
            self.call(host, "put_network", hex=network["network_hex"])
        for node in self.nodes:
            peers = [other.get("advertise", other.get("listen")) for other in self.nodes if "listen" in other and other["name"] != node["name"]]
            self.call(node["host"], "write_peers", name=node["name"], peers=[(text.rsplit(":", 1)[0].strip("[]"), int(text.rsplit(":", 1)[1])) for text in peers])
        self.network_id = re.search(r"network_id=([0-9a-f]{64})", network["info"])[1]
        (self.dir / "network.id").write_text(self.network_id)
        (self.dir / "manifest.toml").write_bytes(self.path.read_bytes())
        if self.loadgen:
            self.call(self.loadgen["host"], "prepare_password")
        print("LAB initialized", self.network_id)

    def doctor(self):
        if not self.network_id:
            raise ValueError("init LAB before doctor")
        for node in self.nodes:
            result = self.call(node["host"], "doctor", name=node["name"], args=self.args(node, True))
            print(node["name"], result["output"].strip())
            if result["code"]:
                raise RuntimeError("LAB doctor failed")

    def up(self):
        self.doctor()
        for node in sorted(self.nodes, key=lambda n: n["role"] != "finalizer"):
            self.call(node["host"], "start", name=node["name"], args=self.args(node))
        print("LAB processes started")

    def down(self, force=False):
        errors = []
        if self.loadgen:
            try:
                self.call(self.loadgen["host"], "kill" if force else "stop", name="loadgen")
            except Exception as exc:
                errors.append(str(exc))
        for node in reversed(self.nodes):
            try:
                self.call(node["host"], "kill" if force else "stop", name=node["name"])
            except Exception as exc:
                errors.append(node["name"]+": "+str(exc))
        if errors:
            raise RuntimeError("; ".join(errors))

    def fail(self, reason):
        self.failures.append(reason)
        raise RuntimeError("invariant: " + reason)

    def ingest(self, node, event):
        if event.get("v") != 1 or not isinstance(event.get("seq"), int) or not isinstance(event.get("run_id"), str):
            self.fail("invalid event schema")
        key = (node, event["run_id"])
        previous = self.sequence.get(key, 0)
        if event["seq"] != previous + 1:
            self.fail("event sequence gap or replay")
        self.sequence[key] = event["seq"]
        if event.get("network_id", self.network_id) != self.network_id:
            self.fail("foreign NetworkID")
        if event.get("safety_halted") or event["event"] == "poa_safety_halt":
            self.fail("PoA safety halt")
        if event["event"] == "node_status":
            old = self.latest.get(node)
            if old and event["height"] < old["height"]:
                self.fail("finalized height regression")
            self.latest[node] = event
            self.check_head(event["height"], event["tip"], event["state_root"])
        if event["event"] == "block_finalized":
            self.check_head(event["height"], event["block_id"], event["state_root"])
            prior=self.node_heads.get(node,0)
            self.node_heads[node]=max(prior,event["height"])
        operation = event.get("operation_id")
        event_name = event["event"]
        for metric, start_name, end_name, identity in (
            ("protection", "content_securing", "content_protected", operation),
            ("repair", "placement_degraded", "placement_repaired", operation),
            ("reconnect", "peer_disconnected", "peer_connected", event.get("peer"))):
            if identity:
                transition_key = (node, metric, identity)
                if event_name == start_name:
                    self.transitions.setdefault(transition_key, event["time_ms"])
                if event_name == end_name:
                    start_time = self.transitions.pop(transition_key, None)
                    if start_time is not None and event["time_ms"] >= start_time:
                        self.transition_latencies[metric].append(event["time_ms"]-start_time)
        if event["event"] == "operation_accepted" and operation:
            self.accepted.setdefault((node, operation), event["time_ms"])
        if event["event"] == "operation_finalized" and operation:
            if "nonce" in event and "account_id" in event:
                nonce_key = (event["account_id"], event["nonce"])
                if self.nonces.setdefault(nonce_key, operation) != operation:
                    self.fail("Identity nonce finalized for two different operations")
            prior = self.finalized.get(operation)
            if prior is not None and prior != event["height"]:
                self.fail("OperationID finalized at two heights")
            self.finalized[operation] = event["height"]
            start = self.accepted.pop((node, operation), None)
            if start is not None and event["time_ms"] >= start:
                self.latencies.append(event["time_ms"] - start)
        if event["event"] == "content_protected" and event.get("replicas", 0) < event.get("target", 1):
            self.fail("Protected below remote target")

    def check_head(self, height, tip, state_root):
        known = self.heads.setdefault(height, (tip, state_root))
        if known != (tip, state_root):
            self.fail("canonical divergence at height " + str(height))

    def collect(self):
        streams = list(self.nodes)
        if self.loadgen:
            for i in range(int(self.loadgen.get("identities", 2))):
                streams.append({"name": f"loadgen-{i}", "host": self.loadgen["host"], "relative_log": f"loadgen/identity-{i}/client.events.jsonl"})
        for node in streams:
            result = self.call(node["host"], "events", name=node["name"], offset=self.offsets.get(node["name"], 0), **({"relative_log": node["relative_log"]} if "relative_log" in node else {}))
            self.offsets[node["name"]] = result["offset"]
            with (self.dir / "timeline.jsonl").open("a") as timeline:
                for event in result["events"]:
                    self.ingest(node["name"], event)
                    timeline.write(json.dumps({"node": node["name"], **event}) + "\n")
        checkpoint = self.dir / "offsets.tmp"
        checkpoint.write_text(json.dumps(self.offsets))
        checkpoint.replace(self.dir / "offsets.json")

    def status(self, quiet=False):
        self.collect()
        for node in self.nodes:
            health = self.call(node["host"], "status", name=node["name"])
            with (self.dir / "resources.jsonl").open("a") as samples:
                samples.write(json.dumps({"time_ms": int(time.time()*1000), "node": node["name"], **health["metrics"]})+"\n")
            event = self.latest.get(node["name"], {})
            if not quiet:
                print(f"{node['name']:24} {node['role']:10} {event.get('height', '-'):>10} peers={event.get('peers', '-')} {'UP' if health['alive'] else 'DOWN'}")

    def chaos(self, scenario, name, chunk_id=None, delay_ms=100, loss_percent=5):
        node = next((n for n in self.nodes if n["name"] == name),None)
        if node is None: raise ValueError("unknown LAB node")
        if scenario in {"provider-loss", "provider-restart", "corruption"} and node["role"] != "provider":
            raise ValueError("scenario requires provider")
        if scenario.startswith("finalizer") and node["role"] != "finalizer":
            raise ValueError("scenario requires finalizer")
        action = "kill" if scenario in {"provider-loss", "finalizer-crash"} else "stop"
        if scenario in {"latency","packet-loss","listener-partition","partition","network-reset"}:
            if "listen" not in node: raise ValueError("network fault requires a LAB listener")
            port=int(node["listen"].rsplit(":",1)[1])
            evidence=self.call(node["host"],"network_reset" if scenario=="network-reset" else "network_fault",
                port=port,delay_ms=delay_ms if scenario=="latency" else 0,
                loss_percent=100 if scenario in {"partition","listener-partition"} else loss_percent if scenario=="packet-loss" else 0,
                all_tcp=scenario=="partition")
        elif scenario == "corruption":
            self.call(node["host"], "corrupt", name=name, chunk_id=chunk_id)
        elif scenario == "start":
            self.call(node["host"], "start", name=name, args=self.args(node))
        elif scenario in {"provider-loss", "provider-restart", "finalizer-restart", "finalizer-crash"}:
            self.call(node["host"], action, name=name)
            if scenario.endswith("restart"):
                self.call(node["host"], "start", name=name, args=self.args(node))
        else:
            raise ValueError("unknown scenario")
        with (self.dir / "chaos.jsonl").open("a") as log:
            log.write(json.dumps({"time_ms": int(time.time()*1000), "scenario": scenario, "node": name,
                **({"evidence":evidence} if scenario in {"latency","packet-loss","listener-partition","partition","network-reset"} else {})}) + "\n")

    def cleanup(self):
        for host,spec in self.hosts.items():
            if spec.get("namespace"): self.call(host,"namespace_cleanup")

    def report(self, complete=False):
        # Standalone report replays immutable evidence rather than reporting empty state.
        timeline = self.dir / "timeline.jsonl"
        if not self.sequence and timeline.exists() and not self.evidence_invalid:
            with timeline.open() as source:
                for line in source:
                    event = json.loads(line)
                    node = event.pop("node")
                    self.ingest(node, event)
            completed = self.dir / "completed.json"
            if completed.exists():
                evidence = json.loads(completed.read_text())
                complete = evidence.get("network_id") == self.network_id
                self.loadgen_passed = evidence.get("loadgen_passed", False)
        completed = self.dir / "completed.json"
        if completed.exists():
            evidence = json.loads(completed.read_text())
            complete = evidence.get("network_id") == self.network_id
            self.loadgen_passed = evidence.get("loadgen_passed", False)
            self.convergence_target=evidence.get("convergence_target")
            complete=bool(complete and isinstance(self.convergence_target,int) and self.convergence_target>0)
            if self.convergence_target is not None and any(self.node_heads.get(node["name"],0)<self.convergence_target for node in self.nodes):
                if "incomplete recovery evidence" not in self.failures:
                    self.failures.append("incomplete recovery evidence")
        latency = sorted(self.latencies)
        pct = lambda p: latency[min(len(latency)-1, int((len(latency)-1)*p))] if latency else None
        report = {"network_id": self.network_id, "result": "FAIL" if self.failures else "PASS" if complete else "INCOMPLETE",
            "failures": self.failures, "nodes": self.latest, "unique_finalized_operations": len(self.finalized),
            "submit_finality_ms": {"p50": pct(.5), "p95": pct(.95), "p99": pct(.99)},
            "latency_samples": len(latency), "verified_heights": len(self.heads),
            "loadgen_passed": self.loadgen_passed,
            "verified_nonces": len(self.nonces),
            "convergence_target":self.convergence_target,"node_verified_heights":self.node_heads,
            "coverage": "canonical heads, height monotonicity, OperationID height uniqueness, finalized Identity nonce uniqueness, Protected threshold, safety halt; exact synthetic file bytes checked by native loadgen"}
        if self.convergence_target is not None:
            report["coverage"] += "; all manifested nodes converge to frozen finalizer height"
        report["resources"]={}
        samples=self.dir/"resources.jsonl"
        if samples.exists():
            if samples.stat().st_size>1<<30:
                raise ValueError("resource evidence exceeds 1GiB ceiling")
            with samples.open() as stream:
                for line in stream:
                    sample=json.loads(line)
                    observed=report["resources"].setdefault(sample["node"],{"samples":0})
                    observed["samples"]+=1
                    for metric in ("disk_free","cpu_ticks","rss_bytes","open_fds"):
                        if metric in sample:
                            value=sample[metric]
                            bounds=observed.setdefault(metric,{"min":value,"max":value})
                            bounds["min"]=min(bounds["min"],value)
                            bounds["max"]=max(bounds["max"],value)
        for metric, samples in self.transition_latencies.items():
            ordered = sorted(samples)
            report[metric+"_ms"] = {"samples":len(ordered), **{label: ordered[min(len(ordered)-1,int((len(ordered)-1)*q))] if ordered else None for label,q in (("p50",.5),("p95",.95),("p99",.99))}}
        target = self.dir / ("report-" + time.strftime("%Y%m%dT%H%M%S", time.gmtime()) + "-" + uuid.uuid4().hex[:8] + ".json")
        target.write_text(json.dumps(report, indent=2))
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        target.with_suffix(".sha256").write_text(digest + "\n")
        print(target, report["result"])
        return report

    def run(self):
        if self.evidence_invalid:
            raise ValueError("stored evidence violates correctness; preserve it and use a fresh LAB directory")
        duration = int(self.config["lab"].get("duration_seconds", 1800))
        if not 1 <= duration <= 86400:
            raise ValueError("duration must be 1..86400 seconds")
        started = time.monotonic()
        (self.dir / "completed.json").unlink(missing_ok=True)
        (self.dir / "failure.json").unlink(missing_ok=True)
        self.failures = []
        baseline = max(self.heads, default=0)
        scheduled = sorted(self.config.get("chaos", []), key=lambda item: int(item["at_seconds"]))
        expected_down = {}
        network_faults = {}
        last_progress=-60
        for fault in scheduled:
            if not 0 <= int(fault["at_seconds"]) < duration:
                raise ValueError("scheduled chaos outside workload duration")
            if fault["scenario"] in {"provider-loss", "finalizer-crash"} and not 1 <= int(fault.get("duration_seconds", 10)) <= 300:
                raise ValueError("fault downtime must be 1..300 seconds")
            if fault["scenario"] in {"latency","packet-loss","listener-partition","partition"}:
                if not 1<=int(fault.get("duration_seconds",10))<=300: raise ValueError("network fault duration must be 1..300 seconds")
                if not 0<=int(fault.get("delay_ms",100))<=10000 or not 0<=int(fault.get("loss_percent",5))<=100:
                    raise ValueError("network fault bounds exceeded")
        if self.loadgen:
            spec = self.hosts[self.loadgen["host"]]
            root = spec["root"].rstrip("/\\")
            sep = "\\" if spec["type"] == "windows" else "/"
            args = [spec["loadgen_binary"], "--network", root+sep+"network.bin", "--data-dir", root+sep+"loadgen",
                "--peer", self.loadgen["peer"], "--password-file", root+sep+"loadgen.password",
                "--duration", str(duration)+"s", "--identities", str(self.loadgen.get("identities",2)),
                "--profile", self.loadgen.get("profile","files"), "--file-size", self.loadgen.get("file_size","1MiB"),
                "--replicas", str(self.loadgen.get("replicas",2)),
                "--operations-per-second", str(self.loadgen.get("operations_per_second",1))]
            if "max_operations" in self.loadgen:
                args += ["--max-operations", str(self.loadgen["max_operations"])]
            self.call(self.loadgen["host"], "start", name="loadgen", args=args)
        try:
            while time.monotonic() - started < duration:
                elapsed = time.monotonic() - started
                for name,deadline in list(network_faults.items()):
                    if elapsed >= deadline:
                        self.chaos("network-reset",name)
                        del network_faults[name]
                for name, deadline in list(expected_down.items()):
                    if elapsed >= deadline:
                        self.chaos("start", name)
                        del expected_down[name]
                while scheduled and elapsed >= int(scheduled[0]["at_seconds"]):
                    pending_node=next(node for node in self.nodes if node["name"]==scheduled[0]["node"])
                    # Slow transports can make several scheduled faults due at
                    # once. Serialize actual network outages in each namespace.
                    if self.hosts[pending_node["host"]].get("namespace") and any(
                        next(node for node in self.nodes if node["name"]==name)["host"]==pending_node["host"]
                        for name in network_faults):
                        break
                    fault = scheduled.pop(0)
                    if fault["node"] in expected_down or fault["node"] in network_faults:
                        raise ValueError("overlapping faults on same node")
                    if fault["scenario"] in {"latency","packet-loss","listener-partition","partition"}:
                        network_faults[fault["node"]]=elapsed+int(fault.get("duration_seconds",10))
                    self.chaos(fault["scenario"], fault["node"], fault.get("chunk_id"),int(fault.get("delay_ms",100)),int(fault.get("loss_percent",5)))
                    if fault["scenario"] in {"provider-loss", "finalizer-crash"}:
                        expected_down[fault["node"]] = time.monotonic()-started + int(fault.get("duration_seconds",10))
                    if fault["scenario"] in {"latency","packet-loss","listener-partition","partition"}:
                        downtime=int(fault.get("duration_seconds",10))
                        network_faults[fault["node"]]=time.monotonic()-started+downtime
                self.status(quiet=True)
                if elapsed-last_progress>=60:
                    print(f"elapsed={int(elapsed)}s verified_heights={len(self.heads)} finalized_operations={len(self.finalized)}",flush=True)
                    last_progress=elapsed
                for node in self.nodes:
                    if node["name"] not in expected_down and not self.call(node["host"], "status", name=node["name"])["alive"]:
                        self.fail("unexpected process exit: " + node["name"])
                time.sleep(2)
            if scheduled:
                self.fail("scheduled chaos did not execute before workload deadline")
            for name in expected_down:
                self.chaos("start", name)
            for name in list(network_faults):
                self.chaos("network-reset",name)
                del network_faults[name]
            if self.loadgen:
                deadline = time.monotonic() + 600
                while time.monotonic() < deadline:
                    self.collect()
                    client = self.call(self.loadgen["host"], "status", name="loadgen")
                    if not client["alive"]:
                        if not re.fullmatch(r"operations=\d+ result=PASS", client["last_line"]):
                            self.fail("native loadgen failed: " + client["last_line"])
                        self.loadgen_passed = True
                        break
                    time.sleep(2)
                if not self.loadgen_passed:
                    self.fail("native loadgen deadline exceeded")
            if not {node["name"] for node in self.nodes}.issubset(self.latest):
                self.fail("missing node telemetry")
            if max(self.heads, default=0) <= baseline:
                self.fail("no advancing finality evidence")
            finalizer=next(node["name"] for node in self.nodes if node["role"]=="finalizer")
            self.convergence_target=self.node_heads.get(finalizer,0)
            if self.convergence_target <= baseline:
                self.fail("no advancing finalizer evidence")
            convergence_deadline=time.monotonic()+120
            while any(self.node_heads.get(node["name"],0)<self.convergence_target for node in self.nodes):
                if time.monotonic()>convergence_deadline: self.fail("nodes failed to converge after fault recovery")
                self.collect()
                for node in self.nodes:
                    if not self.call(node["host"],"status",name=node["name"])["alive"]:
                        self.fail("process exited during recovery: "+node["name"])
                time.sleep(2)
            (self.dir / "completed.json").write_text(json.dumps({"network_id": self.network_id, "loadgen_passed": self.loadgen_passed,"convergence_target":self.convergence_target}))
            self.report(complete=True)
        except BaseException as exc:
            self.failures.append(str(exc))
            (self.dir / "failure.json").write_text(json.dumps(self.failures))
            self.report()
            raise
        finally:
            cleanup_errors=[]
            for name in list(network_faults):
                try:
                    self.chaos("network-reset",name)
                except Exception as exc:
                    cleanup_errors.append("network reset failed for "+name+": "+str(exc))
            if cleanup_errors:
                self.failures.extend(cleanup_errors)
                (self.dir / "failure.json").write_text(json.dumps(self.failures))
                self.report()
                if sys.exc_info()[0] is None:
                    raise RuntimeError("; ".join(cleanup_errors))


def main():
    if len(sys.argv) == 2 and sys.argv[1] == "--worker":
        try:
            print(json.dumps(worker(json.load(sys.stdin))))
        except Exception as exc:
            print(str(exc), file=sys.stderr)
            return 1
        return 0
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["init", "doctor", "up", "status", "run", "chaos", "down", "report","cleanup"])
    parser.add_argument("manifest")
    parser.add_argument("scenario", nargs="?", choices=["provider-loss","provider-restart","corruption",
        "finalizer-restart","finalizer-crash","start","latency","packet-loss","listener-partition","partition","network-reset"])
    parser.add_argument("node", nargs="?")
    parser.add_argument("--chunk-id")
    parser.add_argument("--delay-ms",type=int,default=100)
    parser.add_argument("--loss-percent",type=int,default=5)
    parser.add_argument("--force", action="store_true", help="force termination of owned LAB processes for down")
    opts = parser.parse_args()
    if opts.command=="chaos" and (not opts.scenario or not opts.node):
        parser.error("chaos requires scenario and node")
    if opts.command!="chaos" and (opts.scenario or opts.node):
        parser.error("scenario and node are only valid for chaos")
    lab = Lab(opts.manifest)
    if opts.command == "chaos":
        lab.chaos(opts.scenario, opts.node, opts.chunk_id,opts.delay_ms,opts.loss_percent)
    elif opts.command == "down":
        lab.down(opts.force)
    else:
        getattr(lab, opts.command)()
    return 0


if __name__ == "__main__":
    sys.exit(main())
