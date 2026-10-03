#!/usr/bin/env python3
"""Real CLI/CYP2 acceptance: strict args, read-only doctor, observer and JSONL."""
import hashlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import time


def main():
    binary = str(Path(sys.argv[1]).resolve())
    with tempfile.TemporaryDirectory(prefix="cybou-lab-cli-") as temp:
        root = Path(temp)
        key = root / "key"
        network = "lab"
        def run(*args, ok=True):
            result = subprocess.run([binary, *map(str,args)], capture_output=True, text=True, timeout=90)
            if ok and result.returncode:
                raise AssertionError(result.stderr)
            if not ok and not result.returncode:
                raise AssertionError("invalid command accepted")
            return result
        run("--help")
        run("serve", ok=False)
        # Only compiled networks start: DEVNET, the test-build LAB network, never a file or MAINNET.
        info = run("network", "info", "--network", "devnet").stdout
        assert "bootstrap=51.255.46.58:29461" in info and "network_binding=" in info
        run("network", "info", "--network", "mainnet", ok=False)
        run("network", "info", "--network", root / "network.bin", ok=False)
        run("network", "lab-poa-seed", "--out", key)
        run("network", "info", "--network", network)
        run("observer", "run", "--network", network, "--data-dir", root/"no-policy", "--peer", "127.0.0.1:31001", ok=False)
        assert not (root/"no-policy").exists()
        run("observer", "run", "--network", network, "--data-dir", root/"bad-geo", "--peer", "127.0.0.1:31001",
            "--peer-admission", "france", "--geo-country-csv", root/"missing.csv", "--geo-sha256", "0"*64,
            "--geo-issued-month", "2026-10", ok=False)
        assert not (root/"bad-geo").exists()
        run("observer", "run", "--network", network, "--data-dir", root/"bad", "--peer", "127.0.0.1:31001", "--capacity", "1GiB", "--peer-admission", "lab", ok=False)
        assert not (root/"bad").exists()
        run("network", "info", "--network", network, "--network", network, ok=False)
        with socket.socket() as port:
            port.bind(("127.0.0.1",0))
            number=port.getsockname()[1]
        endpoint=f"127.0.0.1:{number}"
        run("doctor", "--network", network, "--data-dir", root/"finalizer", "--key-file", key, "--listen", endpoint)
        assert not (root/"finalizer").exists()
        wrong=root/"wrong-key"; wrong.write_bytes(os.urandom(32)); wrong.chmod(0o600)
        run("doctor", "--network", network, "--data-dir", root/"finalizer", "--key-file", wrong, ok=False)
        processes=[]
        def start(role, name, extra):
            args=[binary,role,"run","--network",str(network),"--data-dir",str(root/name),"--event-log",str(root/(name+".jsonl")),"--peer-admission","lab",*extra]
            process=subprocess.Popen(args,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            processes.append(process); return process
        def stop(process):
            process.terminate()
            try: process.wait(timeout=15)
            except subprocess.TimeoutExpired: process.kill(); process.wait(timeout=5)
        try:
            finalizer=start("finalizer","finalizer",["--key-file",str(key),"--listen",endpoint,"--block-interval","100ms"])
            observer=start("observer","observer",["--peer",endpoint])
            deadline=time.monotonic()+30
            while time.monotonic()<deadline:
                if observer.poll() is not None or finalizer.poll() is not None:
                    raise AssertionError("daemon failed")
                path=root/"observer.jsonl"
                if path.exists():
                    records=[json.loads(line) for line in path.read_text().splitlines() if line.endswith("}")]
                    if any(e["event"]=="node_status" and e["height"]>=3 for e in records): break
                time.sleep(.1)
            else: raise AssertionError("observer never synced")
            stop(observer); stop(finalizer)
            def digest():
                return {str(p.relative_to(root/"finalizer")):hashlib.sha256(p.read_bytes()).hexdigest()
                        for p in (root/"finalizer").rglob("*") if p.is_file()}
            before=digest()
            run("doctor","--network",network,"--data-dir",root/"finalizer","--key-file",key)
            assert digest()==before,"doctor modified original DB"
            finalizer=start("finalizer","finalizer",["--key-file",str(key),"--listen",endpoint,"--block-interval","100ms"])
            time.sleep(.5); stop(finalizer)
            events=[json.loads(line) for line in (root/"finalizer.jsonl").read_text().splitlines()]
            runs={}
            for event in events:
                assert event["v"]==1
                seq=runs.get(event["run_id"],0)
                assert event["seq"]==seq+1
                runs[event["run_id"]]=event["seq"]
                assert not {"mnemonic","password","filename","mail_body","private_key"}.intersection(event)
            assert len(runs)==2,"restart needs a fresh run id"
            print("operator CLI acceptance passed")
        finally:
            for process in processes:
                if process.poll() is None: stop(process)
    return 0


if __name__=="__main__": sys.exit(main())
