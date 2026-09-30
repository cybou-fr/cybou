"""Real Linux netem isolation acceptance; run as root, never on host links."""
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import time
import unittest

sys.path.insert(0,str(Path(__file__).parents[2]/"tools"))
import cybou_lab_network as network


@unittest.skipUnless(os.name=="posix" and getattr(os,"geteuid",lambda:1)()==0,"Linux root required")
class NamespaceTests(unittest.TestCase):
    def test_partition_and_foreign_process_guard(self):
        original=network.command(["tc","-j","qdisc","show","dev","lo"])
        with tempfile.TemporaryDirectory(prefix="cybou-lab-netem-") as directory:
            root=Path(directory)
            prefix=network.namespace(root,root.name,prepare=True)
            process=None
            foreign=None
            try:
                process=subprocess.Popen(prefix+[sys.executable,"-c","import socket,time; s=socket.socket(); s.bind(('127.0.0.1',33461)); s.listen(); time.sleep(60)"])
                deadline=time.monotonic()+5
                while time.monotonic()<deadline:
                    fields=Path(f"/proc/{process.pid}/stat").read_text().rsplit(")",1)[1].split()
                    if Path(f"/proc/{process.pid}/ns/net").stat().st_ino==Path("/run/netns",root.name).stat().st_ino: break
                    time.sleep(.02)
                (root/"listener.process.json").write_text(json.dumps({"pid":process.pid,"birth":fields[19]}))
                time.sleep(.1)
                foreign=subprocess.Popen(prefix+["sleep","60"])
                time.sleep(.1)
                with self.assertRaisesRegex(ValueError,"outside this LAB"):
                    network.fault(root,root.name,33461,loss_percent=100)
                foreign.terminate(); foreign.wait(timeout=5); foreign=None
                network.fault(root,root.name,33461,loss_percent=100)
                client=subprocess.run(prefix+[sys.executable,"-c","import socket; s=socket.socket(); s.settimeout(1); s.connect(('127.0.0.1',33461))"],capture_output=True,timeout=5)
                self.assertNotEqual(client.returncode,0)
                evidence=network.fault(root,root.name,33461,reset=True)
                self.assertGreater(sum(item.get("drops",0) for item in evidence["qdisc"]),0)
                network.fault(root,root.name,33471,loss_percent=100,all_tcp=True)
                # A port different from the selected listener is also cut off.
                client=subprocess.run(prefix+[sys.executable,"-c","import socket; s=socket.socket(); s.settimeout(1); s.connect(('127.0.0.1',33461))"],capture_output=True,timeout=5)
                self.assertNotEqual(client.returncode,0)
                evidence=network.fault(root,root.name,33471,reset=True)
                self.assertEqual(evidence["fault"]["scope"],"namespace-tcp")
                self.assertGreater(sum(item.get("drops",0) for item in evidence["qdisc"]),0)
                self.assertEqual(network.command(["tc","-j","qdisc","show","dev","lo"]),original)
                client=subprocess.run(prefix+[sys.executable,"-c","import socket; s=socket.socket(); s.settimeout(1); s.connect(('127.0.0.1',33461))"],capture_output=True,timeout=5)
                self.assertEqual(client.returncode,0,client.stderr.decode())
            finally:
                for child in (foreign,process):
                    if child is not None:
                        child.terminate(); child.wait(timeout=5)
                network.cleanup(root,root.name)


if __name__=="__main__": unittest.main()
