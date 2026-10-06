"""Native desktop test: preferences, PATH/argv[0], positional files and overlays.

Run through generic-exec: python3 openstep/tests/launch_smoke.py SOURCE_ROOT [APP].
Requires openstep/guest-fixtures/CPU.COM, built from tests/guest_cpu.s.
Tests a relocated bundle with fresh preferences and no development tools on PATH.
All new data and logs stay in a unique temporary directory.
"""
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil

root = Path(sys.argv[1]).resolve()
app = (Path(sys.argv[2]) if len(sys.argv) > 2 else
       root / 'openstep/output/DOSBox-X.app').resolve() / 'DOSBox-X'
work = Path(tempfile.mkdtemp(prefix='dosbox-launch-'))
home = work / 'home'
home.mkdir()
environment = dict(os.environ, HOME=str(home), PATH=str(app.parent) + ':/usr/bin:/bin')


def run(name, arguments, executable=None, expected=0):
    log = work / (name + '.log')
    with log.open('w') as output:
        process = subprocess.Popen(arguments, executable=executable, cwd=str(work),
                                   env=environment, stdout=output, stderr=output)
        try:
            result = process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise AssertionError(name + ' timed out; see ' + str(log))
    text = log.read_text(errors='replace')
    assert result == expected, (name, result, text[-3000:])
    print(name + ': exit=' + str(result), flush=True)
    return text


options = ['-fastlaunch', '-log-con', '-time-limit', '10']
text = run('first-preferences', [str(app)] + options + ['-c', 'echo FIRST LAUNCH', '-c', 'exit'])
assert 'FIRST LAUNCH' in text
preferences = home / 'Library/Preferences/DOSBox-X/dosbox-x.conf'
assert '[openstep]' in preferences.read_text()
with preferences.open('a') as output:
    output.write('\n[autoexec]\necho PREFERENCES LOADED\nexit\n')
saved = preferences.read_bytes()
text = run('path-preferences', ['DOSBox-X'] + options)
assert 'PREFERENCES LOADED' in text
assert preferences.read_bytes() == saved
text = run('workspace-argv', ['-DOSBox-X'] + options, executable=str(app))
assert 'PREFERENCES LOADED' in text

folder = work / 'DOS files'
folder.mkdir()
cpu = folder / 'CPU.COM'
shutil.copyfile(root / 'openstep/guest-fixtures/CPU.COM', cpu)
text = run('positional-program', [str(app)] + options + ['-defaultconf', '-exit', '--', str(cpu)])
assert 'CPU/FPU/SMC PASS' in text
text = run('secure-startup-mount', [str(app)] + options +
           ['-defaultconf', '-securemode', '-exit', str(cpu)])
assert 'CPU/FPU/SMC PASS' in text
text = run('positional-folder', [str(app), '-defaultconf', '-fastlaunch', '-log-con',
                               '-time-limit', '3', str(folder)])
assert 'Drive C is mounted' in text, text[-3000:]
run('missing-program', [str(app), str(work / 'absent.com')], expected=1)

base, overlay = work / 'base', work / 'overlay'
base.mkdir()
overlay.mkdir()
original = b'BASE CONTENT\r\n'
(base / 'DATA.TXT').write_bytes(original)
commands = ['mount c "' + str(base) + '"', 'mount c "' + str(overlay) + '" -t overlay',
            'echo APPENDED CONTENT>>c:\\DATA.TXT', 'type c:\\DATA.TXT', 'mount -u c', 'exit']
arguments = [str(app)] + options + ['-defaultconf']
for command in commands:
    arguments += ['-c', command]
text = run('overlay-append', arguments)
assert (base / 'DATA.TXT').read_bytes() == original
assert (overlay / 'DATA.TXT').read_bytes() == original + b'APPENDED CONTENT\r\n'
assert preferences.read_bytes() == saved
print('Launch and overlay checks passed; logs: ' + str(work), flush=True)
