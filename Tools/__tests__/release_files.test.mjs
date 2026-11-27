import { test } from 'node:test';
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';
import { readFileSync } from 'node:fs';

const repoRoot = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const read = (...parts) => readFileSync(join(repoRoot, ...parts), 'utf8');

// Tools/package/files.txt: one path per line (relative to bin\Win), '#' comments.
function manifest() {
  return read('Tools', 'package', 'files.txt')
    .split(/\r?\n/)
    .map((line) => line.trim())
    .filter((line) => line && !line.startsWith('#'));
}

// Every DLL ThirdParty/streamline stages beside d3d9.dll must ship, or DLSS / Reflex silently stay off.
test('the release manifest has the client, DXVK and every staged Streamline DLL', () => {
  const files = manifest();
  const cmake = read('ThirdParty', 'streamline', 'CMakeLists.txt');
  const list = cmake.match(/set\(STREAMLINE_DLLS([^)]*)\)/);
  assert.ok(list, 'STREAMLINE_DLLS not found in ThirdParty/streamline/CMakeLists.txt');
  const dlls = list[1].split(/\s+/).filter(Boolean);
  assert.ok(dlls.length > 0);

  for (const required of ['CoA64.exe', 'XvidDecoder.dll', 'x64/d3d9.dll', ...dlls.map((dll) => `x64/${dll}`)]) {
    assert.ok(files.includes(required), `Tools/package/files.txt lacks ${required}`);
  }
});

// Both packagers stage through Tools/package/stage.ps1, which reads the manifest, instead of keeping their own lists.
test('BuildCoA64.bat and the release workflow package from the manifest', () => {
  assert.match(read('Tools', 'package', 'stage.ps1'), /files\.txt/);
  assert.match(read('BuildCoA64.bat'), /Tools\\package\\stage\.ps1/);
  assert.match(read('.github', 'workflows', 'release.yml'), /Tools\/package\/stage\.ps1/);
});
