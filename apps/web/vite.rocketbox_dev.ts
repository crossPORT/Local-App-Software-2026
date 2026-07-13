import { appendFileSync, readFileSync, writeFileSync } from 'node:fs';
import { networkInterfaces } from 'node:os';
import type { Connect } from 'vite';
import type { Plugin, PreviewServer, ViteDevServer } from 'vite';

const EVENT_LOG_FILE = '/tmp/rocketbox-event-logs.log';
const MAX_LOG_BODY_BYTES = 1_000_000;

function collectLanIpv4(): string[] {
  const ips = new Set<string>();
  for (const entries of Object.values(networkInterfaces())) {
    if (!entries) continue;
    for (const entry of entries) {
      if (entry.family === 'IPv4' && !entry.internal) ips.add(entry.address);
    }
  }
  return [...ips].sort();
}

function networkJson(
  res: Connect.ServerResponse,
  server: ViteDevServer | PreviewServer,
): void {
  const port = server.config.server.port ?? 8080;
  const protocol = server.config.server.https ? 'https:' : 'http:';
  res.setHeader('Content-Type', 'application/json');
  res.end(JSON.stringify({ ips: collectLanIpv4(), port, protocol }));
}

function readBody(req: Connect.IncomingMessage, maxBytes: number): Promise<string> {
  return new Promise((resolve, reject) => {
    const chunks: Buffer[] = [];
    let size = 0;
    let aborted = false;
    req.on('data', (chunk: Buffer) => {
      if (aborted) return;
      size += chunk.length;
      if (size > maxBytes) {
        aborted = true;
        resolve(Buffer.concat(chunks).toString('utf8'));
        return;
      }
      chunks.push(chunk);
    });
    req.on('end', () => {
      if (!aborted) resolve(Buffer.concat(chunks).toString('utf8'));
    });
    req.on('error', reject);
  });
}

async function postLog(req: Connect.IncomingMessage, res: Connect.ServerResponse): Promise<void> {
  try {
    const body = await readBody(req, MAX_LOG_BODY_BYTES);
    const parsed = body ? (JSON.parse(body) as { lines?: unknown }) : {};
    const lines = Array.isArray(parsed.lines)
      ? parsed.lines.filter((line): line is string => typeof line === 'string')
      : [];
    if (lines.length > 0) {
      appendFileSync(EVENT_LOG_FILE, `${lines.join('\n')}\n`);
      for (const line of lines) console.log(`[event-log] ${line}`);
    }
  } catch {
    /* swallow */
  }
  res.statusCode = 204;
  res.end();
}

function getLogs(res: Connect.ServerResponse): void {
  res.setHeader('Content-Type', 'text/plain; charset=utf-8');
  try {
    res.end(readFileSync(EVENT_LOG_FILE, 'utf8'));
  } catch {
    res.end('');
  }
}

function clearLogs(res: Connect.ServerResponse): void {
  try {
    writeFileSync(EVENT_LOG_FILE, '');
  } catch {
    /* ignore */
  }
  res.statusCode = 204;
  res.end();
}

function handle(
  req: Connect.IncomingMessage,
  res: Connect.ServerResponse,
  next: Connect.NextFunction,
  server: ViteDevServer | PreviewServer,
): void {
  const path = (req.url ?? '').split('?')[0] ?? '';
  const route = path.replace(/^\/__booth\b/, '/__rocketbox');
  const method = req.method ?? 'GET';
  if (route === '/__rocketbox/network.json') {
    networkJson(res, server);
    return;
  }
  if (route === '/__rocketbox/log' && method === 'POST') {
    void postLog(req, res);
    return;
  }
  if (route === '/__rocketbox/logs' && method === 'GET') {
    getLogs(res);
    return;
  }
  if (
    (route === '/__rocketbox/logs' && method === 'DELETE') ||
    (route === '/__rocketbox/logs/clear' && method === 'POST')
  ) {
    clearLogs(res);
    return;
  }
  next();
}

export function rocketboxDevPlugin(): Plugin {
  return {
    name: 'rocketbox-dev',
    configureServer(server) {
      server.middlewares.use((req, res, next) => handle(req, res, next, server));
    },
    configurePreviewServer(server) {
      server.middlewares.use((req, res, next) => handle(req, res, next, server));
    },
  };
}
