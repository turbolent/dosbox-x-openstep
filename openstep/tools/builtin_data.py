#!/usr/bin/env python3
"""Split large, unmodified builtin byte arrays into native assembler input.

GCC 4.2 needs hundreds of MiB to parse initializer nodes for the DOS utilities.
The small C++ descriptor retains the exact original array size and contents.
Only literal byte arrays are accepted; other source is left untouched.
"""
import pathlib
import re
import sys


def convert(source, base):
    text = source.read_text(encoding='latin1')
    assembly = ['.data\n']
    pattern = re.compile(r'(?:static\s+)?const\s+unsigned\s+char\s+(\w+)\s*\[\s*\]\s*=\s*\{([^{}]*)\}\s*;', re.S)

    def array(match):
        name, initializer = match.groups()
        initializer = re.sub(r'/\*.*?\*/|//[^\n]*', '', initializer, flags=re.S)
        tokens = [t.strip() for t in initializer.split(',') if t.strip()]
        if not tokens or any(not re.fullmatch(r'0[xX][0-9a-fA-F]+|[0-9]+', t) for t in tokens):
            raise ValueError('Nonliteral byte array: ' + name)
        values = [int(t, 16 if t.lower().startswith('0x') else 10) for t in tokens]
        if any(v < 0 or v > 255 for v in values):
            raise ValueError('Byte out of range: ' + name)
        symbol = 'os_builtin_' + re.sub(r'\W', '_', source.stem) + '_' + name
        assembly.append('.align 2\n.globl _%s\n_%s:\n' % (symbol, symbol))
        for start in range(0, len(values), 32):
            assembly.append('.byte ' + ','.join(str(v) for v in values[start:start+32]) + '\n')
        return 'extern "C" const unsigned char %s[%d];\n#define %s %s' % (symbol, len(values), name, symbol)

    text, count = pattern.subn(array, text)
    if not count:
        raise ValueError('No literal byte arrays in ' + str(source))
    pathlib.Path(str(base) + '.cpp').write_text(text, encoding='latin1')
    pathlib.Path(str(base) + '.s').write_text(''.join(assembly), encoding='ascii')


if __name__ == '__main__':
    convert(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]))
