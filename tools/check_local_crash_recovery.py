#!/usr/bin/env python3
"""Kill a BUILD_TESTS-only child at durable checkpoints and verify in a new process."""
import argparse
from pathlib import Path
import queue
import subprocess
import tempfile
import threading


def run_checkpoint(executable: Path, point: str) -> None:
    with tempfile.TemporaryDirectory(prefix="cybou-local-crash-") as temporary:
        root = Path(temporary).resolve()
        (root / "crash-fixture").write_text("isolated synthetic fixture\n", encoding="utf-8")
        child = subprocess.Popen(
            [str(executable), str(root), point, "write"],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
        )
        lines = queue.Queue()

        def read_lines():
            for line in child.stdout:
                lines.put(line.rstrip())
            lines.put(None)

        reader = threading.Thread(target=read_lines, daemon=True)
        reader.start()
        try:
            while True:
                line = lines.get(timeout=90)
                if line is None:
                    raise RuntimeError(f"{point}: child exited before checkpoint ({child.wait()})")
                print(line, flush=True)
                if line == f"READY {point}":
                    break
            child.kill()  # TerminateProcess on Windows, SIGKILL on POSIX.
            child.wait(timeout=15)
            reader.join(timeout=5)
            recovered = subprocess.run(
                [str(executable), str(root), point, "recover"],
                capture_output=True, text=True, timeout=90,
            )
            print(recovered.stdout, end="", flush=True)
            if recovered.returncode:
                raise RuntimeError(f"{point}: recovery failed: {recovered.stderr}")
            if f"PASS {point}" not in recovered.stdout.splitlines():
                raise RuntimeError(f"{point}: recovery produced no acceptance marker")
        finally:
            if child.poll() is None:
                child.kill()
                child.wait(timeout=15)
            reader.join(timeout=5)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    args = parser.parse_args()
    executable = args.executable.resolve(strict=True)
    checkpoints = ("stage", "commit", "submit", "rotation-prepared", "rotation-finalized", "rotation-promoted")
    for checkpoint in checkpoints:
        run_checkpoint(executable, checkpoint)
    print(f"PASS: {len(checkpoints)} forced-termination/restart scenarios")
