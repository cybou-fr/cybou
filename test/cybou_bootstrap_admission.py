#!/usr/bin/env python3
"""Bootstrap CLI rejects expired Geo data before opening a listener."""
from datetime import date
import hashlib
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    binary = str(Path(sys.argv[1]).resolve())
    today = date.today()
    old_year, old_month = today.year, today.month - 2
    if old_month <= 0:
        old_year -= 1
        old_month += 12
    issued_month = f"{old_year:04d}-{old_month:02d}"
    with tempfile.TemporaryDirectory(prefix="cybou-bootstrap-admission-") as temp:
        root = Path(temp)
        csv = root / "country.csv"
        content = b"192.0.2.0,192.0.2.255,FR\n"
        csv.write_bytes(content)
        result = subprocess.run([
            binary, "serve", str(root / "state"), "127.0.0.1", "29461",
            str(root / "unused.crt"), str(root / "unused.key"),
            "--geo-country-csv", str(csv), "--geo-sha256", hashlib.sha256(content).hexdigest(),
            "--geo-issued-month", issued_month,
        ], capture_output=True, text=True, timeout=10)
        assert result.returncode != 0, "expired bootstrap Geo dataset was accepted"
        assert "fresh, integrity-pinned" in result.stderr
        assert not (root / "state").exists(), "bootstrap state opened with expired Geo data"
    print("bootstrap Geo freshness gate passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
