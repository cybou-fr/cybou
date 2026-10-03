#!/usr/bin/env python3
"""DEVNET CLI acceptance against the existing network, without a local signer."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import tempfile
import time

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary')
    parser.add_argument('--geo-country-csv')
    parser.add_argument('--geo-sha256')
    parser.add_argument('--geo-issued-month')
    opts=parser.parse_args()
    binary=str(Path(opts.binary).resolve())
    geo=[opts.geo_country_csv,opts.geo_sha256,opts.geo_issued_month]
    if any(geo) and not all(geo): parser.error('set all three Geo override parameters together')
    admission=['--peer-admission','france']
    if all(geo):
        for name,value in zip(['--geo-country-csv','--geo-sha256','--geo-issued-month'],geo):
            admission.extend([name,value])
    with tempfile.TemporaryDirectory(prefix='cybou-devnet-cli-') as temp:
        root=Path(temp); data=root/'node'; events=root/'events.jsonl'
        def run(*args,ok=True):
            result=subprocess.run([binary,*map(str,args)],capture_output=True,text=True,timeout=90)
            if ok and result.returncode: raise AssertionError(result.stderr)
            if not ok and not result.returncode: raise AssertionError('invalid command accepted')
            return result
        run('--help');run('serve',ok=False);run('provider','run',ok=False)
        info=run('network','info','--network','devnet')
        assert 'bootstrap=51.255.46.58:29461' in info.stdout
        run('network','info','--network','mainnet',ok=False)
        run('network','info','--network',root/'network.bin',ok=False)
        run('network','provision-devnet',ok=False)
        run('doctor','--network','devnet','--data-dir',data)
        assert not data.exists(),'doctor created a DB'
        for _ in range(2):
            previous_runs={json.loads(line)['run_id'] for line in events.read_text().splitlines()} if events.exists() else set()
            process=subprocess.Popen([binary,'node','run','--network','devnet','--data-dir',str(data),
                '--event-log',str(events),*admission],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            try:
                deadline=time.monotonic()+90
                while time.monotonic()<deadline:
                    assert process.poll() is None,'DEVNET node failed'
                    records=[json.loads(line) for line in events.read_text().splitlines() if line.endswith('}')] if events.exists() else []
                    if any(e['run_id'] not in previous_runs and e['event']=='node_status' and e['peers']>0 for e in records):break
                    time.sleep(.1)
                else:raise AssertionError('DEVNET peer was not reached')
            finally:
                process.terminate()
                try:process.wait(timeout=15)
                except subprocess.TimeoutExpired:process.kill();process.wait(timeout=5)
        def digest():
            return {str(p.relative_to(data)):hashlib.sha256(p.read_bytes()).hexdigest()
                for p in data.rglob('*') if p.is_file()}
        before=digest();run('doctor','--network','devnet','--data-dir',data)
        assert digest()==before,'doctor modified the DB'
        runs={}
        for event in map(json.loads,events.read_text().splitlines()):
            assert 'v' not in event
            assert event['seq']==runs.get(event['run_id'],0)+1
            runs[event['run_id']]=event['seq']
            assert not event.get('poa_signer_active',False),'acceptance must not activate PoA'
            assert not {'mnemonic','password','private_key','mail_body','filename'}.intersection(event)
        assert len(runs)==2,'restart needs a fresh event run id'
        print('DEVNET operator CLI acceptance passed')

if __name__=='__main__':main()
