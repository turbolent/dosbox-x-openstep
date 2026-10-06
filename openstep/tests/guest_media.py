"""Run on OPENSTEP through generic-exec; generated fixtures stay in the source tree."""
import os
import subprocess
import sys
import time

root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '.')
fixtures = root + '/openstep/cd-fixtures'
captures = root + '/openstep/captures-test'
os.makedirs(captures, exist_ok=True)
commands = ['@echo on', 'DX-CAPTURE /A /-D TONE.COM']
for media in ['fixture.iso', 'binary.cue', 'wave.cue']:
    commands += ['imgmount d ' + fixtures + '/' + media + ' -t iso',
                 'type d:\\README.TXT']
    if media.endswith('.cue'):
        commands += ['DX-CAPTURE /A /-D CDPLAY PLAY:2 WAIT']
    commands += ['imgmount -u d']
commands += ['echo MEDIA TEST COMPLETE', 'exit']
args = [root + '/openstep/output/dosbox-x', '-defaultconf', '-fastlaunch',
        '-log-con', '-time-limit', '45', '-set', 'cpu core=dynamic',
        '-set', 'dosbox captures=' + captures,
        '-set', 'dosbox show recorded filename=false']
with open(fixtures + '/MEDIA.BAT', 'wb') as batch:
    batch.write(('\r\n'.join(commands) + '\r\n').encode('ascii'))
for command in ['mount c ' + fixtures, 'c:', 'MEDIA.BAT']:
    args += ['-c', command]
with open(root + '/guest-media.log', 'w') as log:
    process = subprocess.Popen(args, cwd=root, stdout=log, stderr=log)
    print('guest_media_pid=%d' % process.pid, flush=True)
    try:
        result = process.wait(timeout=55)
    except subprocess.TimeoutExpired:
        process.terminate()
        try:
            result = process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            result = process.wait()
print('guest_media_exit=%d' % result, flush=True)
text = open(root + '/guest-media.log').read()
print(text)
assert result == 0
assert text.count('OPENSTEP ISO9660 FILE') == 3
assert 'MEDIA TEST COMPLETE' in text
