export const theme = {
  bg: '#0f1419',
  header: '#1a2332',
  panel: '#1a2332',
  iconBox: '#121822',
  row: '#121822',
  rowSelected: '#1c2a38',
  dropZone: '#0f1620',
  track: '#243042',
  monitorBg: '#0f1620',
  monitorGrid: '#1a2838',
  text: '#f0f4f8',
  muted: '#8899aa',
  accent: '#00d4aa',
  ok: '#3ddb8a',
  offlineDot: '#5a6a7a',
  offlineRow: '#10141c',
  offlineDropZone: '#0c1016',
  warn: '#ff9f43',
  error: '#ff6b6b',
  /** Session/handshake activity (announces, offers) — chart + legend */
  sessionBar: '#5b9fd4',
  /** Payload throughput — chart + legend */
  transferBar: '#00d4aa',
  pulseGreen: '#2a9d6f',
} as const;

export type LinkLed = 'offline' | 'announcing' | 'connected' | 'transferring';
