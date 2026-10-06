"""Package native artifacts or the source needed to rebuild this target."""
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tarfile
from tools.check_app import check_app

root = Path(__file__).resolve().parent.parent
output = root / 'openstep' / 'output'
kind = sys.argv[1] if len(sys.argv) == 2 else ''
if kind not in ('app', 'source'):
    raise SystemExit('usage: python3 openstep/package.py app|source')
output.mkdir(exist_ok=True)

if kind == 'app':
    dependencies = check_app((output / 'dosbox-x').read_bytes())
    app = output / 'DOSBox-X.app'
    app.mkdir(exist_ok=True)
    executable = app / 'DOSBox-X'
    shutil.copyfile(output / 'dosbox-x', executable)
    subprocess.run(['/bin/strip', '-S', str(executable)], check=True)
    check_app(executable.read_bytes())
    executable.chmod(0o755)
    for source, target in [('COPYING', 'COPYING'), ('openstep/README-app.txt', 'README.txt'),
                           ('openstep/DOSBox-X.tiff', 'icon.tiff')]:
        shutil.copyfile(root / source, app / target)
        (app / target).chmod(0o644)
    # Explicit files avoid accidentally shipping user configuration or DOS data.
    files = [(app / name, 'DOSBox-X.app/' + name)
             for name in ['DOSBox-X', 'COPYING', 'README.txt', 'icon.tiff']]
    licenses = app / 'Resources' / 'Licenses'
    licenses.mkdir(parents=True, exist_ok=True)
    for source in sorted((root / 'openstep/licenses').iterdir()):
        target = licenses / source.name
        shutil.copyfile(source, target)
        target.chmod(0o644)
        files.append((target, 'DOSBox-X.app/Resources/Licenses/' + source.name))
    print('Standalone runtime: ' + str(len(dependencies)) + ' OPENSTEP system frameworks')
    archive = output / 'DOSBox-X-OPENSTEP-i386.tar.gz'
else:
    files = []
    for directory in ['src', 'include', 'vs/zlib', 'openstep']:
        for current, directories, names in os.walk(str(root / directory)):
            if Path(current) == root / 'openstep':
                directories[:] = [d for d in directories if d in ('include', 'tests', 'tools', 'vendor', 'licenses')]
            directories[:] = sorted(d for d in directories
                                    if d not in ('output', 'work', 'dist', '__pycache__', '.git')
                                    and not d.endswith('.app'))
            for name in sorted(names):
                path = Path(current) / name
                if name.endswith(('.pyc', '.o', '.d', '.log', '.tar.gz')):
                    continue
                files.append((path, 'dosbox-x-openstep/' + path.relative_to(root).as_posix()))
    for name in ['COPYING', 'AUTHORS', 'README.md', 'contrib/icons/dosbox-x.png']:
        files.append((root / name, 'dosbox-x-openstep/' + name))
    archive = output / 'dosbox-x-openstep-source.tar.gz'

temporary = str(archive) + '.tmp.' + str(os.getpid())
try:
    with tarfile.open(temporary, 'w:gz', format=tarfile.USTAR_FORMAT) as package:
        for path, name in files:
            package.add(str(path), arcname=name, recursive=False)
    os.replace(temporary, str(archive))
finally:
    if os.path.exists(temporary):
        os.unlink(temporary)
print('Created ' + str(archive))
