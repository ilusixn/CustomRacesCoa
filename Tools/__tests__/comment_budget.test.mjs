import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { readFileSync } from 'node:fs';
import { LIMIT, trackedFiles, scan } from '../comment_budget.mjs';

// node --test runs from the repo root, but resolve from this file so the test
// works from any cwd.
const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const baselinePath = join(repoRoot, 'Tools', 'comment_budget.json');

const baseline = JSON.parse(readFileSync(baselinePath, 'utf8'));

// A RATCHET, not a flat rule: files are grandfathered at their current worst run.
test('no file exceeds its comment-run budget', () => {
  const found = scan(trackedFiles(repoRoot), repoRoot);
  const regressions = [];

  for (const [file, { max, at }] of Object.entries(found)) {
    const allowed = baseline[file] ?? LIMIT;
    if (max > allowed) {
      regressions.push(
        `${file}:${at} has a ${max}-line comment run (budget ${allowed}). ` +
        `Keep inline comments to ${LIMIT} lines; long-form goes in ` +
        `a dated docs/ page with a one-line pointer.`);
    }
  }

  assert.deepEqual(regressions, [], `\n${regressions.join('\n')}\n`);
});

// The baseline must only ever shrink.
test('baseline has no stale or slack entries', () => {
  const found = scan(trackedFiles(repoRoot), repoRoot);
  const slack = [];

  for (const [file, allowed] of Object.entries(baseline)) {
    const actual = found[file];
    if (!actual) { slack.push(`${file} is in the baseline but not tracked`); continue; }
    if (actual.max < allowed) {
      slack.push(`${file} budget ${allowed} but longest run is now ${actual.max}`);
    }
  }

  assert.deepEqual(slack, [],
    `\nRun: node Tools/comment_budget.mjs --tighten\n${slack.join('\n')}\n`);
});
