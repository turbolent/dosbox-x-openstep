#!/usr/bin/env python3
"""Generate native checksums from original C++ literals, then test linked data.

The test executable links the same builtin objects as DOSBox-X. This catches
assembler, naming, byte ordering and length mistakes in the low-memory build.
Run from the repository root, with the destination C filename as argv[1].
"""
import pathlib
import re
import sys

declarations, checks = [], []
total = 0
for path in sorted(pathlib.Path('src/builtin').glob('*.cpp')):
    source = path.read_text(encoding='latin1')
    source = re.sub(r'/\*.*?\*/|//[^\n]*', '', source, flags=re.S)
    arrays = re.finditer(r'const\s+unsigned\s+char\s+(\w+)\s*\[\s*\]\s*=\s*\{([^{}]*)\}', source, re.S)
    for match in arrays:
        name, content = match.groups()
        values = []
        for token in content.split(','):
            token = token.strip()
            if not token:
                continue
            assert re.fullmatch(r'0[xX][0-9a-fA-F]+|[0-9]+', token), token
            values.append(int(token, 16 if token.lower().startswith('0x') else 8 if token.startswith('0') else 10))
        digest = 2166136261
        for value in values:
            assert 0 <= value <= 255
            digest = ((digest ^ value) * 16777619) & 0xffffffff
        symbol = 'os_builtin_' + re.sub(r'\W', '_', path.stem) + '_' + name
        declarations.append('extern const unsigned char %s[%d];' % (symbol, len(values)))
        checks.append('if (hash(%s, sizeof(%s)) != 0x%08xu) { puts("%s: FAILED"); return 1; }' % (symbol, symbol, digest, name))
        total += len(values)
assert checks
output = '#include <stdio.h>\n' + '\n'.join(declarations) + '''
static unsigned hash(const unsigned char *data, unsigned size) {
    unsigned value = 2166136261u, i;
    for (i = 0; i < size; ++i) value = (value ^ data[i]) * 16777619u;
    return value;
}
int main(void) {
''' + '\n'.join(checks) + '\nprintf("Builtin data: %d arrays, %d bytes verified\\n");\nreturn 0;\n}\n' % (len(checks), total)
pathlib.Path(sys.argv[1]).write_text(output, encoding='ascii')
