import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

/** Repo-root `tests/fixtures` — shared with C++ golden tests. */
export const fixtureRoot = join(dirname(fileURLToPath(import.meta.url)), '../../../tests/fixtures');

export function readFixture(relativePath: string): Uint8Array {
  return new Uint8Array(readFileSync(join(fixtureRoot, relativePath)));
}
