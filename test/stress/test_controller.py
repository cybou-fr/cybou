"""Controller correctness and isolation tests, independent of daemon liveness."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json
import sys
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).parents[2]/"tools"))

spec=importlib.util.spec_from_file_location("stress",Path(__file__).parents[2]/"tools"/"cybou_stress.py")
stress=importlib.util.module_from_spec(spec)
spec.loader.exec_module(stress)


class ControllerTests(unittest.TestCase):
    def test_delayed_poll_serializes_namespace_faults(self):
        with tempfile.TemporaryDirectory() as directory:
            lab=object.__new__(stress.Lab)
            lab.dir=Path(directory); lab.evidence_invalid=False; lab.loadgen=None
            lab.network_id="lab"; lab.failures=[]; lab.heads={0:("tip","root")}
            lab.latest={}; lab.node_heads={}; lab.finalized={}; lab.convergence_target=None
            lab.loadgen_passed=False
            lab.nodes=[{"name":"finalizer","role":"finalizer","host":"shared"},
                       {"name":"a","role":"provider","host":"shared"},
                       {"name":"b","role":"provider","host":"shared"}]
            lab.hosts={"shared":{"namespace":"cybou-lab-test"}}
            lab.config={"lab":{"duration_seconds":20},"chaos":[
                {"at_seconds":1,"scenario":"latency","node":"a","duration_seconds":2},
                {"at_seconds":4,"scenario":"packet-loss","node":"b","duration_seconds":2},
                {"at_seconds":9,"scenario":"partition","node":"a","duration_seconds":2}]}
            clock=[0]; active=[False]; actions=[]
            def chaos(scenario,name,*args):
                if scenario=="network-reset": active[0]=False
                else:
                    self.assertFalse(active[0],"overlapping actual namespace faults")
                    active[0]=True
                actions.append(scenario)
            def collect(*args,**kwargs):
                lab.heads[clock[0]]=("tip","root")
                lab.latest={node["name"]:{} for node in lab.nodes}
                lab.node_heads={node["name"]:clock[0] for node in lab.nodes}
            lab.chaos=chaos; lab.collect=collect; lab.status=collect
            lab.call=lambda *args,**kwargs:{"alive":True}
            lab.report=lambda **kwargs:None
            with patch.object(stress.time,"monotonic",side_effect=lambda:clock[0]), patch.object(stress.time,"sleep",side_effect=lambda _:clock.__setitem__(0,clock[0]+5)):
                lab.run()
            self.assertEqual(actions,["latency","network-reset","packet-loss","network-reset","partition","network-reset"])

    def test_network_fault_refuses_host_and_unbounded_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)/"cybou-lab-network-test"
            stress.worker({"root":str(root),"token":"owner","action":"prepare"})
            with patch.object(stress.lab_network,"command") as command:
                with self.assertRaises(ValueError):
                    stress.worker({"root":str(root),"token":"owner","action":"network_fault","port":30461})
                for port,delay,loss in ((29461,0,0),(30461,10001,0),(30461,0,101)):
                    with self.assertRaises(ValueError): stress.lab_network.fault(root,root.name,port,delay,loss)
                with self.assertRaises(ValueError): stress.lab_network.namespace(root,"default")
                command.assert_not_called()

    def test_report_replays_persisted_evidence(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)
            manifest=root/"lab.toml"
            manifest.write_text('[lab]\nname="replay"\nrun_dir='+json.dumps(str(root))+ '\n[hosts.local]\ntype="local"\nroot="/tmp/cybou-lab-test"\n[[nodes]]\nname="finalizer"\nhost="local"\nrole="finalizer"\n')
            (root/"network.id").write_text("lab")
            events=[{"node":"finalizer","v":1,"run_id":"run","seq":height,"time_ms":height*100,"event":"node_status","height":height,"tip":str(height),"state_root":str(height),"network_id":"lab"} for height in (1,2)]
            events.append({"node":"finalizer","v":1,"run_id":"run","seq":3,"time_ms":201,"event":"block_finalized","height":2,"block_id":"2","state_root":"2","network_id":"lab"})
            (root/"timeline.jsonl").write_text("".join(json.dumps(event)+"\n" for event in events))
            (root/"completed.json").write_text(json.dumps({"network_id":"lab","loadgen_passed":False,"convergence_target":2}))
            lab=stress.Lab(manifest)
            self.assertEqual(lab.report()["result"],"PASS")
            self.assertEqual(len(lab.heads),2)
            (root/"completed.json").write_text(json.dumps({"network_id":"lab","loadgen_passed":False}))
            self.assertEqual(stress.Lab(manifest).report()["result"],"INCOMPLETE")
            (root/"completed.json").write_text(json.dumps({"network_id":"lab","loadgen_passed":False,"convergence_target":3}))
            self.assertEqual(stress.Lab(manifest).report()["result"],"FAIL")
            (root/"failure.json").write_text(json.dumps(["native client failed"]))
            self.assertEqual(stress.Lab(manifest).report()["result"],"FAIL")

    def test_owned_root_boundary(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)/"cybou-lab-test"
            with self.assertRaises(ValueError): stress.worker({"root":str(root),"token":"owner","action":"kill","name":"dev"})
            stress.worker({"root":str(root),"token":"owner","action":"prepare"})
            with self.assertRaises(ValueError): stress.inside(root,root/".."/"dev")
            with self.assertRaises(ValueError): stress.worker({"root":str(root),"token":"different","action":"kill","name":"dev"})

    def test_invariants(self):
        lab=object.__new__(stress.Lab)
        lab.sequence={}; lab.latest={}; lab.heads={}; lab.finalized={}; lab.accepted={}; lab.latencies=[];lab.failures=[];lab.network_id="lab"
        lab.nonces={}; lab.transitions={}; lab.transition_latencies={"protection":[],"repair":[],"reconnect":[]}
        lab.node_heads={}
        def event(seq,**fields):
            return {"v":1,"seq":seq,"run_id":"run","time_ms":100,"event":"node_status",
                "height":10,"tip":"tip","state_root":"root","network_id":"lab",**fields}
        lab.ingest("a",event(1))
        with self.assertRaises(RuntimeError): lab.ingest("b",event(1,tip="fork"))
        with self.assertRaises(RuntimeError): lab.ingest("a",event(2,height=9))
        with self.assertRaises(RuntimeError): lab.ingest("c",event(1,event="content_protected",replicas=1,target=2))
        with self.assertRaises(RuntimeError): lab.ingest("d",event(1,safety_halted=True))
        with self.assertRaises(RuntimeError): lab.ingest("e",event(1,network_id="DEV"))
        lab.ingest("f",event(1,event="operation_finalized",operation_id="one",account_id="account",nonce=1))
        # Confirmation on another full node is valid; a different operation is not.
        lab.ingest("g",event(1,event="operation_finalized",operation_id="one",account_id="account",nonce=1))
        with self.assertRaises(RuntimeError): lab.ingest("h",event(1,event="operation_finalized",operation_id="two",account_id="account",nonce=1))
        lab.ingest("i",event(1,event="content_securing",operation_id="file",time_ms=100))
        lab.ingest("i",event(2,event="content_protected",operation_id="file",time_ms=145,replicas=2,target=2))
        self.assertEqual(lab.transition_latencies["protection"],[45])


if __name__=="__main__": unittest.main()
