"""List the lines of a GlueXML.log that a baseline GlueXML.log does not have.

Usage:
    python Tools/coa/gluelog.py --base CoA/baseline/GlueXML-login.log --new D:/WoW/Coa/Logs/GlueXML.log

Timestamps are stripped; a line repeated more often than in the baseline counts once per extra repeat. The exit
code is the number of new lines.
"""
import argparse
import re
import sys
from collections import Counter

STAMP_RE = re.compile(r'^\d\d/\d\d \d\d:\d\d:\d\d\.\d{3}\s+')


def normalize(line):
    return STAMP_RE.sub('', line.rstrip('\r\n'))


def new_lines(base, new):
    budget = Counter(base)
    out = []
    for line in new:
        if budget[line] > 0:
            budget[line] -= 1
        else:
            out.append(line)
    return out


def read(path):
    with open(path, encoding='utf-8', errors='replace') as f:
        return [normalize(line) for line in f if line.strip()]


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument('--base', required=True)
    p.add_argument('--new', required=True)
    args = p.parse_args(argv)
    lines = new_lines(read(args.base), read(args.new))
    for line in lines:
        print(line)
    return len(lines)


if __name__ == '__main__':
    sys.exit(main())
