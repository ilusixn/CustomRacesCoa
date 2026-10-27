// Longest run of consecutive comment-only lines per file. CLAUDE.md caps inline
// comments at 1-2 lines; long-form belongs in a dated docs/ page. From Helios tools/.
import { readFileSync } from 'node:fs';
import { execSync } from 'node:child_process';

export const LIMIT = 2;

// A plain `dir/**/*.x` pathspec needs a subdirectory, so each root also lists its flat form.
export const GLOBS = [
  'CoA/**/*.cpp', 'CoA/**/*.h', 'CoA/**/*.hlsl',
  'Engine/**/*.cpp', 'Engine/**/*.h',
  'WoW/**/*.cpp', 'WoW/**/*.h',
  'Tools/*.cpp', 'Tools/*.h', 'Tools/*.mjs', 'Tools/**/*.cpp', 'Tools/**/*.h', 'Tools/**/*.mjs',
];

// ThirdParty/ (DXVK, Streamline, patches) stays VERBATIM upstream.
export const EXCLUDE = /(^|\/)(ThirdParty|vendor)\//;

export function trackedFiles(cwd = process.cwd()) {
  const args = GLOBS.map(g => `"${g}"`).join(' ');
  return execSync(`git ls-files --recurse-submodules ${args}`, { cwd, encoding: 'utf8' })
    .split(/\r?\n/).filter(Boolean)
    .filter((f) => !EXCLUDE.test(f));
}

// Counts PROSE runs. Exempt: a run starting at line 1 (file header), and
// doc-annotation lines (`///`, `//!`) which are type info, not prose.
export function longestRun(text) {
  const lines = text.split(/\r?\n/);
  const isDoc = /^\s*(---|\/\/\/|\/\/!)/;
  const isComment = l => !isDoc.test(l) && /^\s*(--|\/\/)/.test(l);
  let run = 0, start = 0, max = 0, at = 0;
  for (let i = 0; i <= lines.length; i++) {
    if (i < lines.length && isComment(lines[i])) {
      if (run === 0) start = i + 1;
      run++;
    } else {
      if (run > max && start > 1) { max = run; at = start; }
      run = 0;
    }
  }
  return { max, at };
}

export function scan(files, cwd = process.cwd()) {
  const out = {};
  for (const f of files) {
    let text;
    try { text = readFileSync(`${cwd}/${f}`, 'utf8'); } catch { continue; }
    out[f] = longestRun(text);
  }
  return out;
}

// --tighten regenerates Tools/comment_budget.json. Entries only ever go DOWN:
// a file that got worse must be fixed, not re-baselined.
if (process.argv[1] && process.argv[1].endsWith('comment_budget.mjs')) {
  const { writeFileSync, readFileSync: rf } = await import('node:fs');
  const path = 'Tools/comment_budget.json';
  let prev = {};
  try { prev = JSON.parse(rf(path, 'utf8')); } catch { /* first run */ }

  const found = scan(trackedFiles());
  const next = {};
  for (const [file, { max }] of Object.entries(found)) {
    if (max <= LIMIT) continue;
    const was = prev[file];
    if (was !== undefined && max > was) {
      console.error(`REFUSED ${file}: ${was} -> ${max}. Budgets only shrink.`);
      process.exit(1);
    }
    next[file] = max;
  }

  const sorted = Object.fromEntries(Object.keys(next).sort().map(k => [k, next[k]]));
  writeFileSync(path, JSON.stringify(sorted, null, 2) + '\n');
  console.log(`comment_budget.json: ${Object.keys(sorted).length} grandfathered files`);
}
