import react from '@vitejs/plugin-react';
import basicSsl from '@vitejs/plugin-basic-ssl';
import { appendFileSync, readFileSync, writeFileSync } from 'node:fs';
import { networkInterfaces } from 'node:os';
import type { Connect } from 'vite';
import { defineConfig, type Plugin, type PreviewServer, type ViteDevServer } from 'vite';
import { VitePWA } from 'vite-plugin-pwa';

const base = process.env.VITE_BASE_PATH ?? '/';

const BOOTH_LOG_FILE = '/tmp/rocketbox-booth-logs.log';
const MAX_LOG_BODY_BYTES = 1_000_000;

function collectLanIpv4(): string[] {
  const ips = new Set<string>();
  for (const entries of Object.values(networkInterfaces())) {
    if (!entries) {
      continue;
    }
    for (const entry of entries) {
      if (entry.family === 'IPv4' && !entry.internal) {
        ips.add(entry.address);
      }
    }
  }
  return [...ips].sort();
}

function boothNetworkMiddleware(
  req: Connect.IncomingMessage,
  res: Connect.ServerResponse,
  server: ViteDevServer | PreviewServer,
): void {
  if (req.url !== '/__booth/network.json') {
    return;
  }
  const port = server.config.server.port ?? 8080;
  const protocol = server.config.server.https ? 'https:' : 'http:';
  res.setHeader('Content-Type', 'application/json');
  res.end(JSON.stringify({ ips: collectLanIpv4(), port, protocol }));
}

function readRequestBody(req: Connect.IncomingMessage, maxBytes: number): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    let aborted = false;
    req.on('data', (chunk: Buffer) => {
      if (aborted) {
        return;
      }
      size += chunk.length;
      if (size > maxBytes) {
        aborted = true;
        resolve(Buffer.concat(chunks).toString('utf8'));
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => {
      if (!aborted) {
        resolve(Buffer.concat(chunks).toString('utf8'));
      }
    });
    req.on('error', reject);
  });
}

async function boothLogPostMiddleware(
  req: Connect.IncomingMessage,
  res: Connect.ServerResponse,
): Promise<void> {
  try {
    const body = await readRequestBody(req, MAX_LOG_BODY_BYTES);
    const parsed = body ? (JSON.parse(body) as { clientId?: string; lines?: unknown }) : {};
    const lines = Array.isArray(parsed.lines)
      ? parsed.lines.filter((line): line is string => typeof line === 'string')
      : [];
    if (lines.length > 0) {
      appendFileSync(BOOTH_LOG_FILE, `${lines.join('\n')}\n`);
      for (const line of lines) {
        console.log(`[booth] ${line}`);
      }
    }
  } catch {
    // Never let log shipping break the dev server; just swallow bad bodies.
  }
  res.statusCode = 204;
  res.end();
}

function boothLogsGetMiddleware(res: Connect.ServerResponse): void {
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
  try {
    res.end(readFileSync(BOOTH_LOG_FILE, 'utf8'));
  } catch {
    res.end('');
  }
}

function boothLogsClearMiddleware(res: Connect.ServerResponse): void {
  try {
    writeFileSync(BOOTH_LOG_FILE, '');
  } catch {
    // Ignore — nothing to clear or not writable.
  }
  res.statusCode = 204;
  res.end();
}

function boothMiddleware(
  req: Connect.IncomingMessage,
  res: Connect.ServerResponse,
  next: Connect.NextFunction,
  server: ViteDevServer | PreviewServer,
): void {
  const url = req.url ?? '';
  const method = req.method ?? 'GET';
  if (url === '/__booth/network.json') {
    boothNetworkMiddleware(req, res, server);
    return;
  }
  if (url === '/__booth/log' && method === 'POST') {
    void boothLogPostMiddleware(req, res);
    return;
  }
  if (url === '/__booth/logs' && method === 'GET') {
    boothLogsGetMiddleware(res);
    return;
  }
  if (url === '/__booth/logs' && method === 'DELETE') {
    boothLogsClearMiddleware(res);
    return;
  }
  if (url === '/__booth/logs/clear' && method === 'POST') {
    boothLogsClearMiddleware(res);
    return;
  }
  next();
}

function boothNetworkPlugin(): Plugin {
  return {
    name: 'booth-network',
    configureServer(server) {
      server.middlewares.use((req, res, next) => {
        boothMiddleware(req, res, next, server);
      });
    },
    configurePreviewServer(server) {
      server.middlewares.use((req, res, next) => {
        boothMiddleware(req, res, next, server);
      });
    },
  };
}

export default defineConfig({
  base,
  plugins: [
    basicSsl(),
    react(),
    boothNetworkPlugin(),
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
          {
            src: 'favicon.svg',
            sizes: 'any',
            type: 'image/svg+xml',
            purpose: 'any',
          },
          {
            src: 'icon-192.png',
            sizes: '192x192',
            type: 'image/png',
            purpose: 'any',
          },
          {
            src: 'icon-512.png',
            sizes: '512x512',
            type: 'image/png',
            purpose: 'any',
          },
          {
            src: 'icon-512.png',
            sizes: '512x512',
            type: 'image/png',
            purpose: 'maskable',
          },
        ],
      },
    }),
  ],
  server: {
    port: 8080,
    host: true,
  },
  preview: {
    port: 8080,
    host: true,
  },
});
