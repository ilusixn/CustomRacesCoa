#!/usr/bin/env node
// PostToolUse(Edit|Write) gate: blocks a file whose longest prose comment run
// exceeds its budget. See Tools/__tests__/comment_budget.test.mjs for the ratchet.
import { readFileSync } from 'node:fs';
import { relative, resolve, sep } from 'node:path';
import { LIMIT, longestRun, EXCLUDE } from './comment_budget.mjs';

const repoRoot = resolve(import.meta.dirname, '..');

function readStdin() {
  try { return readFileSync(0, 'utf8'); } catch { return ''; }
}

let payload;
try { payload = JSON.parse(readStdin()); } catch { process.exit(0); }

const abs = payload?.tool_response?.filePath || payload?.tool_input?.file_path;
if (!abs) process.exit(0);

const rel = relative(repoRoot, resolve(abs)).split(sep).join('/');
// Outside the repo, or a type the convention does not cover.
if (rel.startsWith('..') || !/\.(cpp|h|hlsl|mjs)$/.test(rel)) process.exit(0);
// Third-party source kept verbatim so it stays diffable against upstream.
if (EXCLUDE.test(rel)) process.exit(0);

let text;
try { text = readFileSync(abs, 'utf8'); } catch { process.exit(0); }

let baseline = {};
try {
  baseline = JSON.parse(readFileSync(resolve(repoRoot, 'Tools/comment_budget.json'), 'utf8'));
} catch { /* no baseline yet */ }

// EDITING a file spends its grandfathering: the baseline exempts files nobody
// has touched, not files we are in. Pre-existing comments are ours to fix now.
const { max, at } = longestRun(text);
if (max <= LIMIT) process.exit(0);

const was = baseline[rel];
const legacy = was !== undefined && max <= was;

process.stdout.write(JSON.stringify({
  decision: 'block',
  reason:
    `${rel}:${at} has a ${max}-line comment run (limit ${LIMIT})` +
    (legacy
      ? `. That run is PRE-EXISTING (grandfathered at ${was}), but you edited ` +
        `this file, so the convention now applies to all of it -- fix the ` +
        `existing comments too, not just yours.`
      : '.') +
    ` CLAUDE.md caps inline comments at ${LIMIT} lines: state only the ` +
    `non-obvious "why". Move the reasoning into a dated docs/ page and leave ` +
    `a one-line pointer to it.`,
}));
