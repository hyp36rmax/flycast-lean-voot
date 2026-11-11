# SPDX-License-Identifier: GPL-2.0-or-later
from contextlib import ExitStack
from pathlib import Path
import json, socket, subprocess, sys, time, uuid

root = Path(sys.argv[2]).resolve()
exe = Path(sys.argv[1]).resolve()
out = root / "transport-runs" / uuid.uuid4().hex
out.mkdir(parents=True)
runs = {}

def wait_file(path, peers):
    deadline = time.monotonic() + 10
    while not path.is_file():
        assert all(p.poll() is None for p in peers), str(path)
        assert time.monotonic() < deadline, str(path)
        time.sleep(.02)

def release_ports():
    with ExitStack() as resources:
        for port in (38901, 38902):
            sock = resources.enter_context(socket.socket(socket.AF_INET, socket.SOCK_DGRAM))
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            sock.bind(("127.0.0.1", port))

def run(name, modes, steps, timeout):
    target = out / name
    target.mkdir()
    children = []
    with ExitStack() as resources:
        try:
            for mode, *args in modes:
                log = resources.enter_context((target / (mode + ".log")).open("wb"))
                children.append(subprocess.Popen([str(exe), mode, str(target), *args],
                    stdout=log, stderr=subprocess.STDOUT, creationflags=subprocess.CREATE_NO_WINDOW))
            steps(target, children)
            deadline = time.monotonic() + timeout
            for child in children:
                child.wait(timeout=max(1, deadline - time.monotonic()))
            codes = [child.returncode for child in children]
            assert codes == ([0, 1] if name == "abrupt" else [0] * len(children)), (name,codes)
            release_ports()
            logs = {mode: (target / (mode+".log")).read_text().strip() for mode,*_ in modes}
            runs[name] = {"exit_codes": codes, "pids": [p.pid for p in children], "logs": logs, "ports_released": True}
            print(name, logs, flush=True)
        finally:
            for child in children:
                if child.poll() is None:
                    child.kill()
                child.wait(timeout=5)

def ready(target, children):
    wait_file(target/"MASTER", children)
    wait_file(target/"SLAVE", children)
    (target/"go").write_text("both ready")

def terminate(target, children):
    wait_file(target/"peer-sent", children)
    children[1].kill()

run("sustained", [("master","3600"),("slave","3600")], ready, 80)
run("faults", [("faults",)], lambda *_: None, 20)
run("abrupt", [("wait-exit",),("peer-exit",)], terminate, 12)
run("restart", [("master","120"),("slave","120")], ready, 10)
(root/"transport-results.json").write_text(json.dumps({"evidence":str(out),"runs":runs},indent=2))
