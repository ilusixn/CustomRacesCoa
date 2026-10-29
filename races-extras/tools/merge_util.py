"""Tiny helper for hand merges: LF-normalised edits that keep the file's line endings and fail loudly when an anchor is missing."""


class Merger:
    def __init__(self, path):
        self.path = path
        raw = open(path, 'rb').read().decode('utf-8')
        self.crlf = raw.count('\r\n') > raw.count('\n') / 2
        self.s = raw.replace('\r\n', '\n')

    def replace(self, old, new, optional=False):
        if old not in self.s:
            if optional:
                print('  (skipped) anchor not found:', old[:70].replace('\n', ' '))
                return
            raise SystemExit('ANCHOR NOT FOUND in %s:\n%s' % (self.path, old[:200]))
        self.s = self.s.replace(old, new, 1)

    def save(self):
        out = self.s.replace('\n', '\r\n') if self.crlf else self.s
        open(self.path, 'wb').write(out.encode('utf-8'))
        print('merged', self.path)
