import { defineConfig } from 'vitest/config';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

export default defineConfig({
  resolve: {
    alias: {
      '@rocketbox/sdk': resolve(__dirname, '../../sdks/typescript/src/index.ts'),
    },
  },
  server: {
    fs: {
      allow: [resolve(__dirname, '../..')],
    },
  },
  test: {
    include: ['src/**/*.test.ts', '../../sdks/typescript/src/fabric/**/*.test.ts'],
    environment: 'node',
    typecheck: {
      enabled: false,
    },
  },
});
