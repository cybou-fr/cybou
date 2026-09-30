"""Linux network faults confined to a controller-created LAB namespace."""
import json
import os
from pathlib import Path
import re
import subprocess


def command(args):
    result=subprocess.run(args, capture_output=True, text=True, timeout=15)
    if result.returncode:
        raise RuntimeError("isolated networking: "+result.stderr.strip()[:1000])
    return result.stdout


def namespace(root, name, prepare=False):
    if os.name != "posix" or not Path("/proc/self/ns/net").exists() or os.geteuid() != 0:
        raise ValueError("isolated networking requires a Linux root worker")
    if name != root.name or not re.fullmatch(r"cybou-lab-[a-zA-Z0-9_-]{1,48}", name):
        raise ValueError("namespace must match the owned LAB root name")
    record = root / ".namespace.json"
    path = Path("/run/netns") / name
    if prepare and not record.exists():
        if path.exists():
            raise ValueError("refusing to adopt an existing network namespace")
        command(["ip", "netns", "add", name])
        try:
            command(["ip", "-n", name, "link", "set", "lo", "up"])
            record.write_text(json.dumps({"name":name,"inode":path.stat().st_ino}))
        except BaseException:
            command(["ip", "netns", "delete", name])
            raise
    if not record.exists() or not path.exists():
        raise ValueError("LAB namespace missing; initialize the LAB first")
    saved = json.loads(record.read_text())
    if saved != {"name":name,"inode":path.stat().st_ino} or path.stat().st_ino == Path("/proc/self/ns/net").stat().st_ino:
        raise ValueError("namespace ownership changed or targets the host network")
    return ["ip", "netns", "exec", name]


def isolated_processes(root, name):
    prefix = namespace(root,name)
    inode = (Path("/run/netns")/name).stat().st_ino
    owned = set()
    for file in root.glob("*.process.json"):
        state=json.loads(file.read_text())
        try:
            fields=Path(f'/proc/{state["pid"]}/stat').read_text().rsplit(")",1)[1].split()
            if fields[0] != "Z" and fields[19] == state["birth"]:
                owned.add(state["pid"])
        except FileNotFoundError:
            pass
    live=[]
    for path in Path("/proc").iterdir():
        if not path.name.isdigit(): continue
        try:
            if (path/"ns/net").stat().st_ino == inode:
                pid=int(path.name)
                if pid not in owned:
                    raise ValueError("namespace contains a process outside this LAB")
                live.append(pid)
        except (FileNotFoundError, ProcessLookupError):
            pass
    return prefix,live


def fault(root,name,port,delay_ms=0,loss_percent=0,reset=False,all_tcp=False):
    if not isinstance(all_tcp,bool): raise ValueError("invalid network fault scope")
    if not isinstance(port,int) or not 30000 <= port <= 65535:
        raise ValueError("fault port must be a LAB listener")
    if not isinstance(delay_ms,int) or not 0 <= delay_ms <= 10000 or not isinstance(loss_percent,int) or not 0 <= loss_percent <= 100:
        raise ValueError("network fault bounds exceeded")
    prefix,live=isolated_processes(root,name)
    state=root/".network-fault.json"
    if reset:
        evidence={}
        if state.exists():
            evidence={"fault":json.loads(state.read_text()),"qdisc":json.loads(command(prefix+["tc","-j","-s","qdisc","show","dev","lo"]))}
            command(prefix+["tc","qdisc","del","dev","lo","root"])
            state.unlink()
        return {"reset":True,**evidence}
    if not live: raise ValueError("network fault requires running owned LAB processes")
    if state.exists(): raise ValueError("overlapping network faults are forbidden; reset first")
    # Classify the chosen listener or all TCP, only on owned namespace loopback.
    command(prefix+["tc","qdisc","add","dev","lo","root","handle","1:","prio","bands","3","priomap"]+["0"]*16)
    try:
        command(prefix+["tc","qdisc","add","dev","lo","parent","1:3","handle","30:","netem","delay",f"{delay_ms}ms","loss",f"{loss_percent}%","limit","1000"])
        for protocol,offset in (("ip",0),("ipv6",2)):
            if all_tcp:
                command(prefix+["tc","filter","add","dev","lo","parent","1:","protocol",protocol,"prio",str(1+offset),"flower","ip_proto","tcp","flowid","1:3"])
            else:
                for priority,direction in ((1,"dst_port"),(2,"src_port")):
                    command(prefix+["tc","filter","add","dev","lo","parent","1:","protocol",protocol,"prio",str(priority+offset),"flower","ip_proto","tcp",direction,str(port),"flowid","1:3"])
        evidence={"scope":"namespace-tcp" if all_tcp else "listener","port":port,"delay_ms":delay_ms,"loss_percent":loss_percent}
        state.write_text(json.dumps(evidence))
        return {**evidence,"qdisc":json.loads(command(prefix+["tc","-j","-s","qdisc","show","dev","lo"]))}
    except BaseException:
        command(prefix+["tc","qdisc","del","dev","lo","root"])
        raise


def cleanup(root,name):
    _,live=isolated_processes(root,name)
    if live: raise ValueError("stop LAB processes before namespace cleanup")
    command(["ip","netns","delete",name])
    (root/".namespace.json").unlink()
    (root/".network-fault.json").unlink(missing_ok=True)
    return {"removed":name}
