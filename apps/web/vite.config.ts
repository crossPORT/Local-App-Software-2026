import react from '@vitejs/plugin-react';
import basicSsl from '@vitejs/plugin-basic-ssl';
import { resolve, dirname } from 'node:path';
import { fileURLToPath } from 'node:url';
import { defineConfig } from 'vite';
import { VitePWA } from 'vite-plugin-pwa';
import { rocketboxDevPlugin } from './vite.rocketbox_dev';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const base = process.env.VITE_BASE_PATH ?? '/';

export default defineConfig({
  base,
  resolve: {
    alias: {
      '@rocketbox/sdk': resolve(__dirname, '../../sdks/typescript/src/index.ts'),
    },
  },
  plugins: [
    basicSsl(),
    react(),
    rocketboxDevPlugin(),
    VitePWA({
      registerType: 'autoUpdate',
      includeAssets: ['favicon.svg', 'icon-192.png', 'icon-512.png'],
      manifest: {
        name: 'RocketBox App',
        short_name: 'RocketBox App',
        description: 'SLS USB device file transfer (WebUSB PWA)',
        theme_color: '#1a2332',
        background_color: '#1a2332',
        display: 'standalone',
        start_url: '/app',
        scope: '/',
        icons: [
          { src: 'favicon.svg', sizes: 'any', type: 'image/svg+xml', purpose: 'any' },
          { src: 'icon-192.png', sizes: '192x192', type: 'image/png', purpose: 'any' },
          { src: 'icon-512.png', sizes: '512x512', type: 'image/png', purpose: 'any' },
          { src: 'icon-512.png', sizes: '512x512', type: 'image/png', purpose: 'maskable' },
        ],
      },
    }),
  ],
  server: { port: 8080, host: true },
  preview: { port: 8080, host: true },
});
