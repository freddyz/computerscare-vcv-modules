#!/usr/bin/env python3
"""Real process/lock regression checks; uses a dummy Rack, never the real app."""
import os
from pathlib import Path
import shlex
import signal
import subprocess
import tempfile
import time

REPO = Path(__file__).resolve().parent.parent

def running(pid):
    result = subprocess.run(['ps', '-p', str(pid), '-o', 'stat='], capture_output=True, text=True)
    return bool(result.stdout.strip()) and 'Z' not in result.stdout

def wait_for(predicate, message, seconds=8):
    end = time.monotonic() + seconds
    while time.monotonic() < end:
        if predicate():
            return
        time.sleep(.04)
    raise AssertionError(message)

with tempfile.TemporaryDirectory(prefix='vcvmon-process-') as directory:
    root = Path(directory)
    (root / 'plugin.json').write_text('{}')
    dummy = root / 'dummy.py'
    dummy.write_text('''import os, signal, sys, time
from pathlib import Path
root=Path(sys.argv[1])
(root/'rack-pid').write_text(str(os.getpid()))
with (root/'starts').open('a') as log: log.write('start\\n')
def stop(*args):
    time.sleep(.35)
    (root/'rack-stopped').write_text('stopped')
    sys.exit(0)
signal.signal(signal.SIGTERM,stop)
signal.signal(signal.SIGINT,signal.SIG_IGN)
while True: time.sleep(.05)
''')
    executable = root / 'Rack'
    executable.write_text('#!/bin/zsh\nexec python3 '+shlex.quote(str(dummy))+' '+shlex.quote(str(root))+'\n')
    executable.chmod(0o755)
    harness = root / 'watch.zsh' 
    harness.write_text(f'''source {shlex.quote(str(REPO/'scripts/vcvmon.zsh'))}
VCVMON_REPO={shlex.quote(str(root))}
VCVMON_RACK=$VCVMON_REPO
VCVMON_PAUSE="$VCVMON_REPO/control"
_vcvmon_stamp () {{ print stable; }}
make () {{
  if [[ "$*" == run ]]; then python3 {shlex.quote(str(dummy))} "$VCVMON_REPO"; else return 0; fi
}}
read () {{ sleep .05; return 1; }}
vcvmon dev
''')
    log = (root / 'watch-log').open('w')
    first = subprocess.Popen(['zsh', str(harness)], cwd=REPO, stdout=log, stderr=log, start_new_session=True)
    child = None
    try:
        wait_for(lambda: (root/'rack-pid').exists(), 'Dummy Rack failed to launch')
        child = int((root/'rack-pid').read_text())
        second = subprocess.run(['zsh', str(harness)], cwd=REPO, capture_output=True, text=True, timeout=4)
        assert second.returncode != 0 and 'another watcher' in second.stderr, second.stderr
        assert (root/'starts').read_text().count('start') == 1
        assert running(child)
        os.killpg(first.pid, signal.SIGINT)
        first.wait(timeout=8)
        wait_for(lambda: not running(child), 'Ctrl+C left an orphan Rack process')
        assert (root/'rack-stopped').exists(), 'Rack was not given time to stop cleanly'
        assert (root/'control.status').read_text().startswith('stopped')
    finally:
        if first.poll() is None:
            os.killpg(first.pid, signal.SIGKILL)
            first.wait()
        if child and running(child):
            os.kill(child, signal.SIGKILL)
        log.close()
    # The kernel lock must be released after Ctrl+C, so a new watcher can start.
    (root/'rack-pid').unlink()
    (root/'rack-stopped').unlink()
    with (root/'watch-log2').open('w') as log:
        third = subprocess.Popen(['zsh', str(harness)], cwd=REPO, stdout=log, stderr=log, start_new_session=True)
        child = None
        try:
            wait_for(lambda: (root/'rack-pid').exists(), 'Lock was not released after shutdown')
            child = int((root/'rack-pid').read_text())
            # Rebuild must wait until the old Rack has actually finished exiting.
            (root/'control.command').write_text('rebuild')
            wait_for(lambda: (root/'starts').read_text().count('start') == 3, 'Rebuild did not launch a replacement')
            assert not running(child), 'Rebuild started a replacement before the old Rack exited'
            child = int((root/'rack-pid').read_text())
            os.killpg(third.pid, signal.SIGINT)
            third.wait(timeout=8)
            wait_for(lambda: not running(child), 'Second shutdown left Rack running')
        finally:
            if third.poll() is None:
                os.killpg(third.pid, signal.SIGKILL)
                third.wait()
            if child and running(child):
                os.kill(child, signal.SIGKILL)
    # A hung dev Rack ignores TERM; Ctrl+C must still remove that owned process.
    dummy.write_text(dummy.read_text().replace('signal.signal(signal.SIGTERM,stop)', 'signal.signal(signal.SIGTERM,signal.SIG_IGN)'))
    (root/'rack-pid').unlink()
    with (root/'watch-hung-log').open('w') as log:
        hung = subprocess.Popen(['zsh', str(harness)], cwd=REPO, stdout=log, stderr=log, start_new_session=True)
        child = None
        try:
            wait_for(lambda: (root/'rack-pid').exists(), 'Hung dummy failed to launch')
            child = int((root/'rack-pid').read_text())
            old_child = child
            starts_before = (root/'starts').read_text().count('start')
            (root/'control.command').write_text('rebuild')
            wait_for(lambda: (root/'starts').read_text().count('start') == starts_before + 1, 'Hung dev Rack prevented rebuild', seconds=12)
            assert not running(old_child), 'Rebuild launched before the hung owned Rack was gone'
            child = int((root/'rack-pid').read_text())
            os.killpg(hung.pid, signal.SIGINT)
            hung.wait(timeout=12)
            wait_for(lambda: not running(child), 'Ctrl+C failed to force-stop an owned hung Rack')
        finally:
            if hung.poll() is None:
                os.killpg(hung.pid, signal.SIGKILL)
                hung.wait()
            if child and running(child):
                os.kill(child, signal.SIGKILL)
print('vcvmon singleton, Ctrl+C cleanup, forced rebuild cleanup, lock release and slow Rack restart checks passed')
