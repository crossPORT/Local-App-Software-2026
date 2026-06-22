import react from '@vitejs/plugin-react';
import { defineConfig } from 'vite';
import { VitePWA } from 'vite-plugin-pwa';

const base = process.env.VITE_BASE_PATH ?? '/';

export default defineConfig({
  base,
  plugins: [
    react(),
    VitePWA({
      registerType: 'autoUpdate',
      includeAssets: ['favicon.svg', 'booth-port0.conf', 'booth-port1.conf'],
      manifest: {
        name: 'RocketBox',
        short_name: 'RocketBox',
        description: 'SLS USB device file transfer (WebUSB PWA)',
        theme_color: '#1a2332',
        background_color: '#1a2332',
        display: 'standalone',
        start_url: './',
        icons: [
          {
            src: 'favicon.svg',
            sizes: 'any',
            type: 'image/svg+xml',
            purpose: 'any',
          },
        ],
      },
    }),
  ],
  server: {
    port: 8080,
  },
});
