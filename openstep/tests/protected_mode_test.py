"""Run through generic-exec: python3 protected_mode_test.py SOURCE_ROOT.

Exercises the bundled protected-mode ZIP/UNZIP programs with both CPU cores.
Fixtures and logs stay in a unique directory under openstep/output.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
import zipfile

root = Path(sys.argv[1]).resolve()
work = Path(tempfile.mkdtemp(prefix='protected-mode-', dir=str(root / 'openstep/output')))
for core in ('normal', 'dynamic'):
    folder = work / core
    folder.mkdir()
    payload = (b'OPENSTEP DOSBox-X ZIP regression\r\n' * 1000) + bytes(range(256))
    second = b'Protected mode twice\r\n'
    (folder / 'INPUT.DAT').write_bytes(payload)
    (folder / 'SECOND.TXT').write_bytes(second)
    commands = ['set TZ=UTC0', 'mount c "' + str(folder) + '"', 'c:',
                'z:\\bin\\zip.exe result.zip input.dat',
                'if errorlevel 1 echo ZIP CREATE FAILED',
                'z:\\bin\\zip.exe result.zip second.txt',
                'if errorlevel 1 echo ZIP UPDATE FAILED',
                'z:\\bin\\unzip.exe -t result.zip',
                'if errorlevel 1 echo ZIP TEST FAILED',
                'echo ZIP ROUNDTRIP RETURNED', 'exit']
    args = [str(root / 'openstep/output/dosbox-x'), '-defaultconf', '-fastlaunch',
            '-log-con', '-time-limit', '25', '-set', 'cpu core=' + core,
            '-set', 'cpu cycles=10000']
    for command in commands:
        args += ['-c', command]
    log = work / (core + '.log')
    with log.open('w') as output:
        process = subprocess.Popen(args, cwd=str(root), stdout=output, stderr=output)
        try:
            result = process.wait(timeout=35)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
            raise AssertionError(core + ' timed out; see ' + str(log))
    text = log.read_text(errors='replace')
    assert result == 0, (core, result, str(log))
    assert 'LOG: DOS CON: ZIP ROUNDTRIP RETURNED' in text, str(log)
    assert not any('LOG: DOS CON: ZIP ' + step + ' FAILED' in text
                   for step in ('CREATE', 'UPDATE', 'TEST')), str(log)
    with zipfile.ZipFile(folder / 'RESULT.ZIP') as archive:
        assert archive.testzip() is None
        entries = {name.upper(): archive.read(name) for name in archive.namelist()}
        assert entries == {'INPUT.DAT': payload, 'SECOND.TXT': second}
    print(core + ': ZIP create/update/test and independent decompression passed', flush=True)
print('Protected-mode regression passed; logs: ' + str(work), flush=True)
