import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { readFileSync } from 'node:fs';
import { execSync } from 'node:child_process';
import { LIMIT, longestRun, EXCLUDE } from '../comment_budget.mjs';

// node --test runs from the repo root, but resolve from this file so the test
// works from any cwd.
const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..');

// The per-file ratchet only guards a file's MAXIMUM run, so a long new comment
// in a file already grandfathered high passes it. This reads the diff.
const COVERED = /\.(cpp|h|hlsl|mjs)$/;
const IS_DOC = /^\s*(---|\/\/\/|\/\/!)/;
const IS_COMMENT = (l) => !IS_DOC.test(l) && /^\s*(--|\/\/)/.test(l);

// Defaults to the change under review (last commit + working tree), NOT the
// branch base: that would re-flag whole imported trees.
function diffAgainstBase() {
  const base = process.env.COMMENT_DIFF_BASE || 'HEAD~1';
  try {
    return execSync(`git diff --unified=0 ${base}`,
      { cwd: repoRoot, encoding: 'utf8', maxBuffer: 64 * 1024 * 1024 });
  } catch {
    return '';   // no such base (first commit, shallow clone)
  }
}

// Walks added ('+') lines only, so an untouched legacy block never trips this.
function addedCommentRuns(diff) {
  const offenders = [];
  let file = null, run = 0, firstLine = null, lineNo = 0;

  const flush = () => {
    if (run > LIMIT && file) {
      offenders.push(`${file}:${firstLine} adds a ${run}-line comment run`);
    }
    run = 0; firstLine = null;
  };

  for (const raw of diff.split('\n')) {
    if (raw.startsWith('+++ ')) {
      flush();
      const m = raw.match(/^\+\+\+ b\/(.+)$/);
      // Vendored upstream source stays byte-for-byte upstream; exempt it here
      // too, same as the sibling test below.
      file = m && COVERED.test(m[1]) && !EXCLUDE.test(m[1]) ? m[1] : null;
      continue;
    }
    if (raw.startsWith('@@')) {
      flush();
      const m = raw.match(/\+(\d+)/);
      lineNo = m ? Number(m[1]) : 0;
      continue;
    }
    if (!file || !raw.startsWith('+')) { flush(); continue; }

    const body = raw.slice(1);
    if (IS_COMMENT(body)) {
      if (run === 0) firstLine = lineNo;
      run++;
    } else {
      flush();
    }
    lineNo++;
  }
  flush();
  return offenders;
}

test('no commit adds a comment run longer than the limit', () => {
  const diff = diffAgainstBase();
  if (!diff) return;                       // nothing to compare against
  const offenders = addedCommentRuns(diff);

  assert.deepEqual(offenders, [],
    `\nCLAUDE.md caps inline comments at ${LIMIT} lines. Move the reasoning into ` +
    `a dated docs/ page and leave a one-line pointer:\n  ` +
    offenders.join('\n  ') + '\n');
});

// Editing a file spends its grandfathering. The baseline exempts files nobody
// has touched; once we are in one, its pre-existing comments are ours to fix.
test('a file this change touches is clean as a whole', () => {
  const diff = diffAgainstBase();
  if (!diff) return;

  const touched = [...diff.matchAll(/^\+\+\+ b\/(.+)$/gm)].map(m => m[1])
    .filter(f => COVERED.test(f))
    .filter(f => !EXCLUDE.test(f));

  const dirty = [];
  for (const f of [...new Set(touched)]) {
    let text;
    try { text = readFileSync(join(repoRoot, f), 'utf8'); } catch { continue; }
    const { max, at } = longestRun(text);
    if (max > LIMIT) dirty.push(`${f}:${at} still has a ${max}-line run`);
  }

  assert.deepEqual(dirty, [],
    `\nThese files were edited, so the ${LIMIT}-line cap applies to ALL their ` +
    `comments -- including ones we did not write:\n  ` + dirty.join('\n  ') + '\n');
});
