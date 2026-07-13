import { createServer } from 'net';
import { createServer as createHttpServer } from 'http';
import { WebSocketServer } from 'ws';

const TCP_PORT = Number(process.env.ROCKETBOX_TCP_PORT || 1772);
const WS_PORT = Number(process.env.ROCKETBOX_WS_PORT || 1773);
const LISTEN_HOST = process.env.ROCKETBOX_LISTEN_HOST || '0.0.0.0';

// Message Types
const MSG_ATTACH = 0x01;
const MSG_LIST = 0x02;
const MSG_CONNECT = 0x03;
const MSG_DISCONNECT = 0x04;
const MSG_KEEPALIVE = 0x05;
const MSG_SESSION = 0x09;

const MSG_ATTACHED = 0x81;
const MSG_SYSTEMS = 0x82;
const MSG_ACK = 0x83;
const MSG_NAK = 0x84;
const MSG_CIRCUIT_UP = 0x85;
const MSG_CIRCUIT_DOWN = 0x86;
const MSG_STATUS = 0x87;

// NAK Reason codes
const NAK_BUSY = 0x01;
const NAK_DOWN = 0x02;
const NAK_DENIED = 0x03;
const NAK_INVALID = 0x04;
const NAK_TIMEOUT = 0x05;

// CIRCUIT_DOWN Reason codes
const DOWN_CLOSED = 0x01;
const DOWN_RESET = 0x02;

class VirtualPort {
  constructor(id) {
    this.id = id;
    this.status = 'offline'; // 'offline' | 'reachable' | 'busy'
    this.name = `Port ${id + 1}`;
    this.systemId = `sys-port-${id + 1}`;
    this.socket = null; // raw TCP or WS socket
    this.type = null; // 'tcp' | 'ws'
    this.connectedTo = -1; // index of remote port (0-3)
    this.lastAnnounce = null; // cached MSG_SESSION announce payload
    this.lastDataTime = 0; // timestamp of last raw data packet routed (EP1/EP2)
    this.lastConnectTime = 0; // timestamp of last crossbar switch connection
    this.lastConnectedTo = -1; // remote port index of last connection
  }

  reset() {
    this.status = 'offline';
    this.name = `Port ${this.id + 1}`;
    this.socket = null;
    this.type = null;
    this.connectedTo = -1;
    this.lastAnnounce = null;
    this.lastDataTime = 0;
    this.lastConnectTime = 0;
    this.lastConnectedTo = -1;
  }
}

const ports = Array.from({ length: 4 }, (_, i) => new VirtualPort(i));

/** State-1 default pairs: 1↔2, 3↔4 when both ends are online and unlinked. */
function applyState1DefaultPairs() {
  const pairs = [[0, 1], [2, 3]];
  for (const [a, b] of pairs) {
    const pa = ports[a];
    const pb = ports[b];
    if (pa.status === 'offline' || pb.status === 'offline') continue;
    if (pa.connectedTo !== -1 || pb.connectedTo !== -1) continue;
    pa.connectedTo = b;
    pb.connectedTo = a;
    pa.lastConnectedTo = b;
    pb.lastConnectedTo = a;
    logEvent(`State-1 default pair: Port ${a + 1} <═══> Port ${b + 1}`);
  }
}

// --- Live Dashboard Visualization ---
const logHistory = [];
const controlEvents = []; // visual control events queue for dashboard tracing

function visualLength(str) {
  return str.replace(/\x1b\[[0-9;]*m/g, '').length;
}

function centerVisual(str, targetWidth) {
  const len = visualLength(str);
  if (len >= targetWidth) return str;
  const leftPad = Math.floor((targetWidth - len) / 2);
  const rightPad = targetWidth - len - leftPad;
  return ' '.repeat(leftPad) + str + ' '.repeat(rightPad);
}

function logEvent(msg) {
  const time = new Date().toLocaleTimeString();
  logHistory.push(`[${time}] ${msg}`);
  if (logHistory.length > 100) {
    logHistory.shift();
  }
  renderDashboard();
}

function renderDashboard() {
  // Clear the terminal screen beautifully
  process.stdout.write('\x1Bc');

  const bold = '\x1b[1m';
  const reset = '\x1b[0m';
  const green = '\x1b[32m';
  const blue = '\x1b[34m';
  const yellow = '\x1b[33m';
  const red = '\x1b[31m';
  const gray = '\x1b[90m';
  const cyan = '\x1b[36m';

  console.log(`${bold}${cyan}┌────────────────────────────────────────────────────────────────────────┐${reset}`);
  console.log(`${bold}${cyan}│                     ROCKETBOX SIMULATED HARDWARE                       │${reset}`);
  console.log(`${bold}${cyan}│                Request Point & Crossport Switching Block               │${reset}`);
  console.log(`${bold}${cyan}└────────────────────────────────────────────────────────────────────────┘${reset}`);
  console.log('');

  // Render 4 Ports side-by-side (perfectly padded to 16 visual chars inside the border)
  const colWidth = 16;
  const lines = [
    '  ', // line 0
    '  ', // line 1
    '  ', // line 2
    '  ', // line 3
    '  ', // line 4
    '  '  // line 5
  ];

  for (let i = 0; i < 4; i++) {
    const p = ports[i];
    let statusStr = '';
    let statusColor = '';
    if (p.status === 'offline') {
      statusStr = '● OFFLINE';
      statusColor = gray;
    } else if (p.status === 'reachable') {
      statusStr = '● ONLINE';
      statusColor = green;
    } else {
      statusStr = '● ACTIVE';
      statusColor = red;
    }

    const sysId = p.status !== 'offline' ? p.systemId : '---';
    const typeStr = (p.status !== 'offline' && p.type) ? `[${p.type.toUpperCase()}]` : '---';

    lines[0] += `┌${'─'.repeat(colWidth)}┐   `;
    lines[1] += `│${centerVisual(`${bold}${cyan}PORT ${p.id + 1}${reset}`, colWidth)}│   `;
    lines[2] += `│${centerVisual(`${statusColor}${statusStr}${reset}`, colWidth)}│   `;
    lines[3] += `│${centerVisual(sysId, colWidth)}│   `;
    lines[4] += `│${centerVisual(typeStr, colWidth)}│   `;
    lines[5] += `└${'─'.repeat(colWidth)}┘   `;
  }

  lines.forEach(l => console.log(l));
  console.log('');

  console.log(`${bold}${blue}┌────────────────────────────────────────────────────────────────────────┐${reset}`);
  console.log(`${bold}${blue}│               CROSSPORT MATRIX (DATA SWITCHING FLIGHT)                 │${reset}`);
  console.log(`${bold}${blue}└────────────────────────────────────────────────────────────────────────┘${reset}`);

  let hasCircuits = false;
  for (let i = 0; i < 4; i++) {
    const p = ports[i];
    if (p.connectedTo !== -1 && p.id < p.connectedTo) {
      const peer = ports[p.connectedTo];
      console.log(`   ${bold}${green}Port ${p.id + 1} (${p.systemId}) <══════════[ ACTIVE CIRCUIT (EP1/EP2) ]══════════> Port ${peer.id + 1} (${peer.systemId})${reset}`);
      hasCircuits = true;
    }
  }
  if (!hasCircuits) {
    console.log(`   ${gray}No active hardware matrix circuits. Data plane is currently idle.${reset}`);
  }
  console.log('');

  console.log(`${bold}${yellow}┌────────────────────────────────────────────────────────────────────────┐${reset}`);
  console.log(`${bold}${yellow}│                           HARDWARE ACTIVITY LOG                        │${reset}`);
  console.log(`${bold}${yellow}└────────────────────────────────────────────────────────────────────────┘${reset}`);
  
  if (logHistory.length === 0) {
    console.log(`   ${gray}Waiting for events...${reset}`);
  } else {
    logHistory.slice(-6).forEach(line => {
      console.log(`   ${line}`);
    });
  }
  console.log('');
}

function sendControlPacket(port, type, txn, arg, payload = Buffer.alloc(0)) {
  const header = Buffer.alloc(12);
  header.writeUInt8(0x01, 0); // ver
  header.writeUInt8(type, 1);
  header.writeUInt16BE(txn, 2);
  header.writeUInt32BE(arg, 4);
  header.writeUInt32BE(payload.length, 8);

  const packet = Buffer.concat([Buffer.from([0x03]), header, payload]); // 0x03 prefix is EP3 Control IN

  if (port.type === 'tcp' && !port.socket.destroyed) {
    port.socket.write(packet);
  } else if (port.type === 'ws' && port.socket.readyState === 1) {
    port.socket.send(packet);
  }
}

function handleCrossbarSwitchRequest(port, destPort) {
  logEvent(`[Port ${port.id + 1}] Crossbar switch request: Link to Port ${destPort}`);
  
  if (destPort >= 1 && destPort <= 4) {
    const targetIdx = destPort - 1;
    const targetPort = ports[targetIdx];

    // HW EP4 parity: empty dest is not an error (no NAK). Record intent only.
    if (targetPort.status === 'offline') {
      logEvent(`[Port ${port.id + 1}] Switch to offline Port ${destPort} (accepted, silence)`);
      if (port.connectedTo !== -1 && port.connectedTo !== targetIdx) {
        const prev = ports[port.connectedTo];
        if (prev && prev.connectedTo === port.id) {
          prev.connectedTo = -1;
          if (prev.status === 'busy') prev.status = 'reachable';
        }
      }
      port.connectedTo = targetIdx;
      port.lastConnectedTo = targetIdx;
      return;
    }
    
    // Wire them together in the switching matrix (bidirectional).
    if (port.connectedTo !== -1 && port.connectedTo !== targetIdx) {
      const prev = ports[port.connectedTo];
      if (prev && prev.connectedTo === port.id) {
        prev.connectedTo = -1;
        if (prev.status === 'busy') prev.status = 'reachable';
      }
    }
    port.status = port.status === 'offline' ? 'reachable' : port.status;
    if (port.status === 'reachable') port.status = 'busy';
    targetPort.status = 'busy';
    port.connectedTo = targetPort.id;
    targetPort.connectedTo = port.id;

    const now = Date.now();
    port.lastConnectTime = now;
    port.lastConnectedTo = targetPort.id;
    targetPort.lastConnectTime = now;
    targetPort.lastConnectedTo = port.id;
    
    logEvent(`Crossbar Matrix Connected: Port ${port.id + 1} <═══> Port ${port.connectedTo + 1}`);
    // No ACK/CIRCUIT_UP for HW-style presence path (silence model).
    broadcastSystems();
  } else {
    // Disconnect
    const peerIdx = port.connectedTo;
    port.status = 'reachable';
    port.connectedTo = -1;
    
    // Reply ACK to initiator
    sendControlPacket(port, MSG_ACK, 0, 0);
    
    if (peerIdx !== -1) {
      const peer = ports[peerIdx];
      if (peer) {
        peer.status = 'reachable';
        peer.connectedTo = -1;
        sendControlPacket(peer, MSG_CIRCUIT_DOWN, 0, DOWN_CLOSED);
      }
    }
    
    logEvent(`Crossbar Matrix Disconnected: Port ${port.id + 1} disconnected`);
    broadcastSystems();
  }
}

function handleControlMessage(portRef, header, payload) {
  let port = portRef.current;
  const type = header.readUInt8(1);
  const txn = header.readUInt16BE(2);
  const arg = header.readUInt32BE(4);

  logEvent(`[Port ${port.id + 1}] Received MSG type: 0x${type.toString(16)}, txn: ${txn}, arg: ${arg}`);

  switch (type) {
    case MSG_ATTACH: {
      let reqPort = arg;
      if (reqPort < 0 || reqPort >= 4) {
        reqPort = port.id; // use pre-assigned port
      }
      const targetPort = ports[reqPort];
      if (targetPort.status !== 'offline' && targetPort.socket !== port.socket) {
        sendControlPacket(port, MSG_NAK, txn, NAK_BUSY);
        return;
      }

      // Claim the port
      if (targetPort !== port) {
        targetPort.socket = port.socket;
        targetPort.type = port.type;
        targetPort.status = 'reachable';
        port.reset();
        port = targetPort;
        portRef.current = targetPort; // Update the reference so other listeners use the correct port!
      } else {
        port.status = 'reachable';
      }

      logEvent(`[Port ${port.id + 1}] Successfully Attached!`);
      sendControlPacket(port, MSG_ATTACHED, txn, port.id);
      broadcastSystems();
      break;
    }

    case MSG_LIST: {
      sendControlPacket(port, MSG_SYSTEMS, txn, 0, buildSystemsPayload());
      break;
    }

    case MSG_CONNECT: {
      if (port.status !== 'reachable') {
        sendControlPacket(port, MSG_NAK, txn, NAK_INVALID);
        return;
      }
      let targetSysId = payload.toString().trim();
      let targetPort = ports.find(p => p.systemId === targetSysId);
      if (!targetPort && arg >= 0 && arg < 4) {
        targetPort = ports[arg];
      }

      if (!targetPort || targetPort.status === 'offline') {
        sendControlPacket(port, MSG_NAK, txn, NAK_DOWN);
        return;
      }
      if (targetPort.status === 'busy') {
        sendControlPacket(port, MSG_NAK, txn, NAK_BUSY);
        return;
      }

      // Establish Circuit
      port.status = 'busy';
      targetPort.status = 'busy';
      port.connectedTo = targetPort.id;
      targetPort.connectedTo = port.id;

      const connectNow = Date.now();
      port.lastConnectTime = connectNow;
      port.lastConnectedTo = targetPort.id;
      targetPort.lastConnectTime = connectNow;
      targetPort.lastConnectedTo = port.id;

      logEvent(`Circuit Connected: Port ${port.id + 1} <-> Port ${targetPort.id + 1}`);

      sendControlPacket(port, MSG_ACK, txn, 0, Buffer.from(targetPort.systemId));
      sendControlPacket(targetPort, MSG_CIRCUIT_UP, 0, 0, Buffer.from(port.systemId));

      broadcastSystems();
      break;
    }

    case MSG_DISCONNECT: {
      if (port.status !== 'busy' || port.connectedTo === -1) {
        sendControlPacket(port, MSG_NAK, txn, NAK_INVALID);
        return;
      }
      const peer = ports[port.connectedTo];
      port.status = 'reachable';
      port.connectedTo = -1;

      sendControlPacket(port, MSG_ACK, txn, 0);

      if (peer) {
        peer.status = 'reachable';
        peer.connectedTo = -1;
        sendControlPacket(peer, MSG_CIRCUIT_DOWN, 0, DOWN_CLOSED);
      }

      logEvent(`Circuit Disconnected: Port ${port.id + 1}`);
      broadcastSystems();
      break;
    }

    case MSG_KEEPALIVE: {
      sendControlPacket(port, MSG_ACK, txn, 0);
      break;
    }

    case MSG_SESSION: {
      const str = payload.toString('utf8');
      
      // Enqueue visual control-plane event for the real-time visualizer
      controlEvents.push({
        id: `${Date.now()}-${Math.random().toString(36).substr(2, 6)}`,
        sender: port.id + 1,
        timestamp: Date.now(),
        isAnnounce: str.includes('kind=announce')
      });
      if (controlEvents.length > 30) {
        controlEvents.shift();
      }

      if (str.includes('kind=announce')) {
        port.lastAnnounce = payload;
        const fromMatch = str.match(/^from=(.+)$/m);
        if (fromMatch) {
          const newName = fromMatch[1].trim();
          if (newName && port.name !== newName) {
            logEvent(`[Port ${port.id + 1}] Updating simulated hardware system name to: "${newName}"`);
            port.name = newName;
            broadcastSystems();
          }
        }
      }
      ports.forEach(p => {
        if (p.status !== 'offline' && p.id !== port.id) {
          sendControlPacket(p, MSG_SESSION, txn, arg, payload);
        }
      });
      break;
    }

    default:
      logEvent(`[Port ${port.id + 1}] Unknown MSG type: 0x${type.toString(16)}`);
      break;
  }
}

function handleClientDisconnect(port) {
  if (port.status === 'offline') return;
  logEvent(`[Port ${port.id + 1}] Client disconnected`);
  
  const peerId = port.connectedTo;
  port.reset();

  if (peerId !== -1) {
    const peer = ports[peerId];
    if (peer && peer.status === 'busy') {
      peer.status = 'reachable';
      peer.connectedTo = -1;
      sendControlPacket(peer, MSG_CIRCUIT_DOWN, 0, DOWN_RESET);
    }
  }

  broadcastSystems();
}

function handleDataPacket(port, data) {
  if (port.status !== 'busy' || port.connectedTo === -1) {
    logEvent(`[Port ${port.id + 1}] Data packet ignored - no active circuit`);
    return;
  }
  const peer = ports[port.connectedTo];
  if (!peer || peer.status !== 'busy') {
    return;
  }

  // Record data transfer timestamps for visual rendering
  const now = Date.now();
  port.lastDataTime = now;
  peer.lastDataTime = now;

  // Prepend 0x02 (EP2 Data IN) + 4-byte BE length to the raw payload bytes
  const header = Buffer.alloc(5);
  header.writeUInt8(0x02, 0);
  header.writeUInt32BE(data.length, 1);
  const dataPacket = Buffer.concat([header, data]);

  if (peer.type === 'tcp' && !peer.socket.destroyed) {
    peer.socket.write(dataPacket);
  } else if (peer.type === 'ws' && peer.socket.readyState === 1) {
    peer.socket.send(dataPacket);
  }
  
  logEvent(`Data Routed: Port ${port.id + 1} (${data.length} bytes) ──> Port ${peer.id + 1}`);
}

function buildSystemsPayload() {
  const activePorts = ports.filter(p => p.status !== 'offline');
  const buffers = [Buffer.alloc(4)];
  buffers[0].writeUInt32BE(activePorts.length, 0);

  activePorts.forEach(p => {
    const idBuf = Buffer.from(p.systemId);
    const nameBuf = Buffer.from(p.name);
    const item = Buffer.alloc(1 + idBuf.length + 1 + nameBuf.length + 1);
    
    let offset = 0;
    item.writeUInt8(idBuf.length, offset++);
    idBuf.copy(item, offset);
    offset += idBuf.length;
    
    item.writeUInt8(nameBuf.length, offset++);
    nameBuf.copy(item, offset);
    offset += nameBuf.length;
    
    const statusVal = p.status === 'busy' ? 2 : 1;
    item.writeUInt8(statusVal, offset);
    
    buffers.push(item);
  });
  
  return Buffer.concat(buffers);
}

function broadcastSystems() {
  const payload = buildSystemsPayload();
  ports.forEach(p => {
    if (p.status !== 'offline') {
      sendControlPacket(p, MSG_SYSTEMS, 0, 0, payload);
    }
  });
}

function replayAnnouncements(port) {
  ports.forEach(p => {
    if (p.id !== port.id && p.status !== 'offline' && p.lastAnnounce) {
      sendControlPacket(port, MSG_SESSION, 0, 0, p.lastAnnounce);
    }
  });
}

// --- TCP Server for C++ ---
const tcpServer = createServer(socket => {
  logEvent(`New TCP connection established`);
  let port = ports.find(p => p.status === 'offline');
  if (!port) {
    logEvent(`Refused TCP connection - all ports full`);
    socket.destroy();
    return;
  }

  port.socket = socket;
  port.type = 'tcp';
  port.status = 'reachable';
  const activePortRef = { current: port };

  applyState1DefaultPairs();
  replayAnnouncements(port);
  broadcastSystems();

  let recvBuffer = Buffer.alloc(0);

  socket.on('data', data => {
    recvBuffer = Buffer.concat([recvBuffer, data]);

    while (recvBuffer.length > 0) {
      const epId = recvBuffer.readUInt8(0);
      
      if (epId === 0x04) {
        // Detect if this is a Mode 11 Crossbar switch 16-byte packet (1B prefix + 16B packet)
        // Standard protocol ver is 0x01 in the first byte of payload, but message type is never 0x00
        if (recvBuffer.length >= 3) {
          const verByte = recvBuffer.readUInt8(1);
          const typeByte = recvBuffer.readUInt8(2);
          if (verByte !== 0x01 || typeByte === 0x00) {
            if (recvBuffer.length < 17) {
              break; // Wait for full 16-byte switch packet
            }
            const destPort = recvBuffer.readUInt8(1) & 0x0F;
            recvBuffer = recvBuffer.subarray(17);
            handleCrossbarSwitchRequest(activePortRef.current, destPort);
            continue;
          }
        } else if (recvBuffer.length === 2) {
          // If we only have 2 bytes, and the first byte of payload is 0x01, we don't know if it's
          // standard control ver 0x01 or a Switch Request to Port 1 (destPort 0x01).
          // We must wait for the 3rd byte (type byte) to disambiguate.
          break;
        }

        if (recvBuffer.length < 13) {
          break;
        }
        const len = recvBuffer.readUInt32BE(9);
        const totalPacketLen = 1 + 12 + len;
        if (recvBuffer.length < totalPacketLen) {
          break;
        }

        const header = recvBuffer.subarray(1, 13);
        const payload = recvBuffer.subarray(13, totalPacketLen);
        recvBuffer = recvBuffer.subarray(totalPacketLen);

        handleControlMessage(activePortRef, header, payload);
      } else if (epId === 0x01) {
        if (recvBuffer.length < 5) {
          break;
        }
        const len = recvBuffer.readUInt32BE(1);
        const totalPacketLen = 1 + 4 + len;
        if (recvBuffer.length < totalPacketLen) {
          break;
        }

        const dataPayload = recvBuffer.subarray(5, totalPacketLen);
        recvBuffer = recvBuffer.subarray(totalPacketLen);

        handleDataPacket(activePortRef.current, dataPayload);
      } else {
        logEvent(`TCP Error: Invalid EP ID 0x${epId.toString(16)} received`);
        socket.destroy();
        break;
      }
    }
  });

  socket.on('close', () => {
    handleClientDisconnect(activePortRef.current);
  });

  socket.on('error', err => {
    logEvent(`TCP Socket Error: ${err.message}`);
    socket.destroy();
  });
});

tcpServer.listen(TCP_PORT, LISTEN_HOST, () => {
  logEvent(`TCP Server listening on ${LISTEN_HOST}:${TCP_PORT}`);
});

// --- WebSocket Server and Web Dashboard for PWA ---
const wsServer = new WebSocketServer({ noServer: true });

function getHtmlDashboard() {
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8" />
<meta name="viewport" content="width=device-width, initial-scale=1.0" />
<title>RocketBox Switching Block - Live Simulation</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Space+Grotesk:wght@400;500;600;700&family=IBM+Plex+Mono:wght@400;500;600&display=swap" rel="stylesheet">
<style>
  :root{
    --ground:#0f172a; --panel:#1e293b; --panel-line:#334155;
    --orange:#38bdf8; --orange-hi:#0ea5e9;
    --data:#22c55e; --data-dim:#15803d; --data-soft:#064e3b;
    --req:#94a3b8; --req-dim:#475569;
    --ctrl:#38bdf8; --nak:#ef4444;
    --host:#c084fc; --host-dim:#581c87;
    --block:#020617; --block-line:#1e293b;
    --ink:#f8fafc; --ink-mid:#cbd5e1; --ink-low:#64748b;
    --yellow:#f59e0b;
  }
  *{box-sizing:border-box;}
  html,body{margin:0;height:100%;background-color:var(--ground);color:var(--ink);font-family:"Space Grotesk", system-ui, sans-serif;-webkit-font-smoothing:antialiased;}
  .wrap{width:100%; max-width:1060px; margin:0 auto; padding:26px 22px 40px; display:flex; flex-direction:column; min-height:100%; gap: 1.5rem;}
  header{display:flex; align-items:baseline; justify-content:space-between; gap:16px; flex-wrap:wrap; border-bottom:1px solid var(--panel-line); padding-bottom:14px;}
  h1{font-size:20px; font-weight:600; letter-spacing:-0.01em; margin:0; color:var(--orange);}
  h1 span{color:var(--ink-low); font-weight:400;}
  .meta{font-family:"IBM Plex Mono", monospace; font-size:11px; letter-spacing:0.08em; color:var(--ink-low); text-transform:uppercase;}
  
  .stage{background:var(--block); border:1px solid var(--panel-line); border-radius:14px; padding:10px 10px 4px; position:relative;}
  svg{display:block; width:100%; height:auto;}
  
  .console-bar{display:flex; align-items:center; justify-content:space-between; gap:16px; flex-wrap:wrap; background:var(--panel); border:1px solid var(--panel-line); border-radius:8px; padding:12px 16px;}
  .caption{font-family:"IBM Plex Mono", monospace; font-size:13px; color:var(--ink-mid); display:flex; align-items:center; gap:10px; min-height:20px;}
  .caption .dot{width:8px;height:8px;border-radius:50%; background:var(--green); box-shadow:0 0 10px var(--green); flex:0 0 auto;}
  .caption b{color:var(--ink); font-weight:500;}
  
  .btn-reset {
    background-color: var(--nak);
    color: white;
    border: none;
    padding: 0.6rem 1.2rem;
    font-weight: bold;
    border-radius: 6px;
    cursor: pointer;
    font-family: "IBM Plex Mono", monospace;
    font-size: 11px;
    letter-spacing: 0.05em;
    transition: background-color 0.2s, transform 0.1s;
  }
  .btn-reset:hover { background-color: #b91c1c; }
  .btn-reset:active { transform: scale(0.97); }

  .log-section {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    min-height: 250px;
    flex-grow: 1;
  }
  .log-title {
    font-size: 11px;
    font-family: "IBM Plex Mono", monospace;
    letter-spacing: 0.1em;
    font-weight: bold;
    color: var(--yellow);
    text-transform: uppercase;
  }
  .log-console {
    background-color: #020617;
    border: 1px solid var(--panel-line);
    border-radius: 8px;
    font-family: "SFMono-Regular", Consolas, "Liberation Mono", Menlo, monospace;
    font-size: 0.85rem;
    padding: 1rem;
    overflow-y: auto;
    height: 220px;
    display: flex;
    flex-direction: column;
    gap: 0.3rem;
    box-shadow: inset 0 2px 8px rgba(0, 0, 0, 0.8);
  }
  .log-entry { line-height: 1.4; white-space: pre-wrap; word-break: break-all; }
  .log-time { color: var(--ink-low); margin-right: 0.5rem; }
  .log-text { color: #e2e8f0; }
  .log-text.highlight { color: var(--orange); font-weight: bold; }
  .log-text.success { color: var(--data); }
  .log-text.error { color: var(--nak); }
  .log-text.session { color: var(--host); font-weight: 500; }
  .log-text.data-route { color: var(--yellow); font-weight: bold; }

  .legend{display:grid; grid-template-columns:repeat(auto-fit,minmax(230px,1fr)); gap:14px 26px; border-top:1px solid var(--panel-line); padding-top:18px;}
  .leg{font-size:13px; line-height:1.45; color:var(--ink-mid);}
  .leg h3{font-family:"IBM Plex Mono", monospace; font-size:11px; letter-spacing:0.1em; text-transform:uppercase; margin:0 0 6px; display:flex; align-items:center; gap:8px;}
  .swatch{width:22px;height:3px;border-radius:2px; display:inline-block;}
  .leg b{color:var(--ink); font-weight:500;}

  .port-box{fill:var(--panel); stroke:var(--panel-line); stroke-width:1.6;}
  .port-glow{fill:none; stroke:var(--orange); stroke-width:2.4; opacity:0;}
  .port-label{font-family:"Space Grotesk",monospace; font-weight:700; font-size:13px; fill:var(--ink);}
  .port-sub-label{font-family:"IBM Plex Mono",monospace; font-size:9px; fill:var(--ink-low);}
  .pad-num{font-family:"IBM Plex Mono",monospace; font-size:9px; fill:var(--ink-mid);}
  .pad{stroke-width:1.4;}
  .pad.data{fill:var(--data-soft); stroke:var(--data-dim);}
  .pad.host{fill:var(--host-dim); stroke:#701a75;}
  .ep-tag{font-family:"IBM Plex Mono",monospace; font-size:8px; letter-spacing:.02em; fill:var(--ink-low);}

  .fab-box{fill:#020617; stroke:var(--panel-line); stroke-width:1.6;}
  .fab-title{font-family:"IBM Plex Mono",monospace; font-weight:600; font-size:13px; letter-spacing:0.06em; fill:var(--data);}
  .fab-sub{font-family:"IBM Plex Mono",monospace; font-size:10px; fill:var(--ink-low);}
  .rp-box{fill:#020617; stroke:var(--panel-line); stroke-width:1.6;}
  .rp-title{font-family:"IBM Plex Mono",monospace; font-weight:600; font-size:13px; letter-spacing:0.06em; fill:var(--orange);}
  .rp-sub{font-family:"IBM Plex Mono",monospace; font-size:10px; fill:var(--ink-low);}
  .fab-glow{fill:var(--data); opacity:0;}
  .rp-glow{fill:var(--orange); opacity:0;}

  .link-data{fill:none; stroke:var(--panel-line); stroke-width:1.5; opacity:.7;}
  .link-req{fill:none; stroke:var(--panel-line); stroke-width:1.5; opacity:.7;}
  .link-ctrl{fill:none; stroke:var(--ctrl); stroke-width:1.8; opacity:.55;}
  .ctrl-tag{font-family:"IBM Plex Mono",monospace; font-size:9px; letter-spacing:0.08em; fill:var(--ctrl);}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <h1>RocketBox Switching Block <span>/ live hardware simulation</span></h1>
    <div class="meta" id="header-meta">4 ports · EP1/2 data · EP3/4 host</div>
  </header>

  <div class="stage">
    <svg id="scene" viewBox="0 0 960 452">
      <defs>
        <marker id="arrow" viewBox="0 0 8 8" refX="6.5" refY="4" markerWidth="5" markerHeight="5" orient="auto">
          <path d="M0,0 L8,4 L0,8 z" fill="var(--ctrl)"/>
        </marker>
      </defs>
      <g id="block-layer"></g>
      <g id="link-layer"></g>
      <g id="bridge-layer"></g>
      <g id="fx-layer"></g>
      <g id="ports-layer"></g>
      <g id="flow-layer"></g>
    </svg>
  </div>

  <div class="console-bar">
    <div class="caption"><span class="dot" id="capdot" style="background-color: var(--green); box-shadow: 0 0 10px var(--green);"></span><span id="caption">Idle. EP3/EP4 are internal to each port's host. EP1/EP2 face the crossport fabric.</span></div>
    <button class="btn-reset" onclick="resetDevice()">Restart Simulated Device</button>
  </div>

  <section class="log-section">
    <div class="log-title">Live Hardware Console Traffic</div>
    <div class="log-console" id="log-console"></div>
  </section>

  <div class="legend">
    <div class="leg">
      <h3 style="color:var(--orange)"><span class="swatch" style="background:var(--orange)"></span>Request point (top)</h3>
      Each port sends a connection request up the control bus. The request point returns <b>ACK</b> or <b>NAK</b>, and <span style="color:var(--ctrl)">CTRL</span>s the fabric to connect the two ports.
    </div>
    <div class="leg">
      <h3 style="color:var(--host)"><span class="swatch" style="background:var(--host)"></span>Ports · EP3/EP4 (internal)</h3>
      EP4 (host→port) and EP3 (port→host) are internal to the host. The fabric never touches them. The port reads the host packet on EP4 and replies on EP3.
    </div>
    <div class="leg">
      <h3 style="color:var(--data)"><span class="swatch" style="background:var(--data)"></span>Crossport block · the fabric (bottom)</h3>
      <b>EP1</b>/<b>EP2</b> data. The fabric wires a dedicated circuit straight between the two connected ports, on demand.
    </div>
  </div>
</div>

<script>
  (() => {
    "use strict";
    const svgNS="http://www.w3.org/2000/svg";

    // ---- geometry (top: request point, middle: ports, bottom: fabric) -----
    const PORTS=[{id:1,cx:165},{id:2,cx:390},{id:3,cx:615},{id:4,cx:840}];
    const PORT_W=150, PORT_TOP=150, PORT_H=118, PORT_BOT=PORT_TOP+PORT_H;
    const EP_TOP_Y=PORT_TOP+22, EP_BOT_Y=PORT_BOT-20, EP_DX=28;
    const CTR_Y=PORT_TOP+PORT_H/2, LABEL_Y=PORT_TOP+35;
    const RP={x:90,y:28,w:780,h:64};
    const RP_BOT=RP.y+RP.h;
    const CB={x:90,y:340,w:780,h:90};
    const CTRL_X=502;
    const cxOf=id=>PORTS.find(p=>p.id===id).cx;
    const epPos=(id,ep)=>{ const cx=cxOf(id);
      if(ep===4) return {x:cx-EP_DX,y:EP_TOP_Y};
      if(ep===3) return {x:cx+EP_DX,y:EP_TOP_Y};
      if(ep===1) return {x:cx-EP_DX,y:EP_BOT_Y};
      return {x:cx+EP_DX,y:EP_BOT_Y}; };

    const el=id=>document.getElementById(id);
    const blockLayer=el("block-layer"), linkLayer=el("link-layer"), bridgeLayer=el("bridge-layer");
    const fxLayer=el("fx-layer"), portsLayer=el("ports-layer"), flowLayer=el("flow-layer");
    const captionEl=el("caption");
    const capDotEl=el("capdot");
    function mk(t,a){ const n=document.createElementNS(svgNS,t); for(const k in a) n.setAttribute(k,a[k]); return n; }
    const say=(h, color="var(--green)")=>{
      captionEl.innerHTML=h;
      capDotEl.style.backgroundColor = color;
      capDotEl.style.boxShadow = "0 0 10px " + color;
    };

    // ---- request point (top) + crossport block (bottom) -------------------
    blockLayer.appendChild(mk("rect",{class:"rp-box",x:RP.x,y:RP.y,width:RP.w,height:RP.h,rx:10}));
    const rpGlow=mk("rect",{class:"rp-glow",x:RP.x,y:RP.y,width:RP.w,height:RP.h,rx:10}); blockLayer.appendChild(rpGlow);
    blockLayer.appendChild(mk("text",{class:"rp-title",x:RP.x+16,y:RP.y+26})).textContent="REQUEST POINT";
    blockLayer.appendChild(mk("text",{class:"rp-sub",x:RP.x+16,y:RP.y+44})).textContent="ACK / NAK · controller";

    blockLayer.appendChild(mk("rect",{class:"fab-box",x:CB.x,y:CB.y,width:CB.w,height:CB.h,rx:10}));
    const fabGlow=mk("rect",{class:"fab-glow",x:CB.x,y:CB.y,width:CB.w,height:CB.h,rx:10}); blockLayer.appendChild(fabGlow);
    blockLayer.appendChild(mk("text",{class:"fab-title",x:CB.x+16,y:CB.y+24})).textContent="CROSSPORT BLOCK";
    blockLayer.appendChild(mk("text",{class:"fab-sub",x:CB.x+16,y:CB.y+42})).textContent="the fabric · EP1/EP2";

    // CTRL line: request point -> fabric
    blockLayer.appendChild(mk("path",{class:"link-ctrl",d:"M " + CTRL_X + " " + RP_BOT + " L " + CTRL_X + " " + CB.y,"marker-end":"url(#arrow)"}));
    blockLayer.appendChild(mk("text",{class:"ctrl-tag",x:CTRL_X+8,y:(RP_BOT+CB.y)/2,"text-anchor":"start"})).textContent="CTRL";

    // ---- standing links: request up, data down (straight) -----------------
    PORTS.forEach(p=>{
      linkLayer.appendChild(mk("path",{class:"link-req",d:"M " + p.cx + " " + PORT_TOP + " L " + p.cx + " " + RP_BOT}));
      linkLayer.appendChild(mk("path",{class:"link-data",d:"M " + p.cx + " " + PORT_BOT + " L " + p.cx + " " + CB.y}));
    });

    // ---- ports -------------------------------------------------------------
    const portEls={};
    PORTS.forEach(p=>{
      const x0=p.cx-PORT_W/2;
      const g=mk("g",{class:"port-hit","data-port":p.id});
      g.appendChild(mk("rect",{class:"port-box",x:x0,y:PORT_TOP,width:PORT_W,height:PORT_H,rx:8}));
      const glow=mk("rect",{class:"port-glow",x:x0-3,y:PORT_TOP-3,width:PORT_W+6,height:PORT_H+6,rx:10}); g.appendChild(glow);
      
      const lbl = mk("text",{class:"port-label",x:p.cx,y:LABEL_Y,"text-anchor":"middle"});
      lbl.textContent="PORT "+p.id;
      g.appendChild(lbl);

      const subLbl = mk("text",{class:"port-sub-label",x:p.cx,y:LABEL_Y+16,"text-anchor":"middle"});
      subLbl.textContent="OFFLINE";
      g.appendChild(subLbl);

      const aliasLbl = mk("text",{class:"port-sub-label",x:p.cx,y:LABEL_Y+30,"text-anchor":"middle"});
      aliasLbl.textContent="---";
      aliasLbl.style.fill = "var(--orange)";
      g.appendChild(aliasLbl);

      [[4,"host","host"],[3,"host","host"],[1,"data",""],[2,"data",""]].forEach(([ep,cls,tag])=>{
        const pp=epPos(p.id,ep);
        g.appendChild(mk("rect",{class:"pad " + cls,x:pp.x-10,y:pp.y-6.5,width:20,height:13,rx:3,"data-pad":p.id + "-" + ep}));
        g.appendChild(mk("text",{class:"pad-num",x:pp.x,y:pp.y+3,"text-anchor":"middle"}).cloneNode(true)).textContent=ep;
        if(tag){ g.appendChild(mk("text",{class:"ep-tag",x:pp.x,y:pp.y-11,"text-anchor":"middle"})).textContent=tag; }
      });
      portsLayer.appendChild(g);
      portEls[p.id]={glow, subLbl, aliasLbl, box: g.querySelector(".port-box")};
    });
    const getPad=(id,ep)=>portsLayer.querySelector('[data-pad="' + id + '-' + ep + '"]');

    // ---- WAAPI plumbing ----------------------------------------------------
    const tracked=new Set();
    function track(a){ 
      if(!a) return a; 
      tracked.add(a); 
      if (a.finished) {
        a.finished.then(()=>tracked.delete(a)).catch(()=>{}); 
      } else {
        setTimeout(() => tracked.delete(a), a.effect ? a.effect.getTiming().duration : 1000);
      }
      return a; 
    }
    function wait(ms){ 
      return new Promise(resolve => setTimeout(resolve, ms));
    }
    function sampleTransforms(pathEl,K,rev){ 
      const out=[]; 
      try {
        const L=pathEl.getTotalLength();
        for(let i=0;i<=K;i++){ 
          const t=rev?1-i/K:i/K; 
          const pt=pathEl.getPointAtLength(t*L); 
          out.push({transform:"translate(" + pt.x + "px, " + pt.y + "px)"}); 
        } 
      } catch (e) {
        for(let i=0;i<=K;i++){
          const t=rev?1-i/K:i/K;
          out.push({transform:"translate(" + (100 + t * 760) + "px, 380px)"});
        }
      }
      return out; 
    }

    function flashPad(id,ep,color){ const p=getPad(id,ep); if(!p) return; const base=(ep===1||ep===2)?"var(--data-soft)":"var(--host-dim)";
      track(p.animate([{fill:base},{fill:color},{fill:base}],{duration:640,easing:"ease-out"})); }
    function portGlow(id,on){ const g=portEls[id].glow; g.animate([{opacity:getComputedStyle(g).opacity},{opacity:on?1:0}],{duration:280,fill:"forwards"}); }
    function boxPulse(glow,color){ if(color) glow.setAttribute("fill",color); track(glow.animate([{opacity:0},{opacity:.5},{opacity:0}],{duration:640,easing:"ease-out"})); }

    function pulseAlong(d,opts){
      const color=opts.color||"var(--req)";
      const dur=opts.dur||520;
      const reverse=opts.reverse||false;
      const r=opts.r||5;
      const tmp=mk("path",{d:d,fill:"none",stroke:"none"}); fxLayer.appendChild(tmp);
      const keys=sampleTransforms(tmp,28,reverse); tmp.remove();
      const dot=mk("circle",{r:r,cx:0,cy:0,fill:color}); dot.style.filter="drop-shadow(0 0 6px " + color + ")";
      fxLayer.appendChild(dot);
      const a=dot.animate(keys,{duration:dur,easing:"ease-in-out"}); track(a);
      const donePromise = a.finished ? a.finished.then(()=>dot.remove()).catch(()=>dot.remove()) : new Promise(resolve => {
        setTimeout(() => { dot.remove(); resolve(); }, dur);
      });
      return donePromise;
    }

    // ---- path builders (all clean straight lines) -------------------------
    const hostReadD=id=>{ const c=cxOf(id); return "M " + (c-EP_DX) + " " + EP_TOP_Y + " L " + c + " " + CTR_Y; };
    const hostReplyD=id=>{ const c=cxOf(id); return "M " + c + " " + CTR_Y + " L " + (c+EP_DX) + " " + EP_TOP_Y; };
    const reqD=id=>"M " + cxOf(id) + " " + PORT_TOP + " L " + cxOf(id) + " " + RP_BOT;
    const ctrlD=()=>"M " + CTRL_X + " " + RP_BOT + " L " + CTRL_X + " " + CB.y;
    const dipFor=(a,b)=>CB.y+18+Math.min(Math.abs(cxOf(a)-cxOf(b)),700)/700*44;
    function stapleD(a,b,y){ const ax=cxOf(a),bx=cxOf(b); return "M " + ax + " " + PORT_BOT + " L " + ax + " " + y + " L " + bx + " " + y + " L " + bx + " " + PORT_BOT; }

    // ---- data circuit on the fabric ---------------------------------------
    function drawData(a,b){
      const y=dipFor(a,b);
      const path=mk("path",{d:stapleD(a,b,y),fill:"none",stroke:"var(--data)","stroke-width":2.4,"stroke-linecap":"round","stroke-linejoin":"round",opacity:0.95});
      bridgeLayer.appendChild(path);
      let len = 0;
      try {
        len = path.getTotalLength();
      } catch (e) {
        len = Math.abs(cxOf(a) - cxOf(b)) + 200;
      }
      path.style.strokeDasharray=len; path.style.strokeDashoffset=len;
      const a1=path.animate([{strokeDashoffset:len},{strokeDashoffset:0}],{duration:560,easing:"ease-out",fill:"forwards"}); track(a1);
      const donePromise = a1.finished ? a1.finished.catch(()=>{}) : Promise.resolve();
      return {path,len,done:donePromise};
    }
    function flowDots(lane,color,n,reverse){ const dots=[],keys=sampleTransforms(lane.path,46,reverse);
      for(let i=0;i<n;i++){ const dot=mk("circle",{r:3.4,cx:0,cy:0,fill:color}); dot.style.filter="drop-shadow(0 0 5px " + color + ")"; flowLayer.appendChild(dot);
        const anim=dot.animate(keys,{duration:1700,iterations:Infinity,easing:"linear",delay:-(i/n)*1700}); track(anim); dots.push({dot,a:anim}); }
      return dots; }

    // ---- State Sync Pipeline ----
    const portState={1:"offline",2:"offline",3:"offline",4:"offline"};
    const portNames={1:"---",2:"---",3:"---",4:"---"};
    const bridges={};

    function syncHardwareState(ports, logHistory, controlEvents) {
      let headerCount = 0;

      // 1. Update text fields and state styles for each port
      for (let i = 0; i < ports.length; i++) {
        const remote = ports[i];
        const id = remote.id + 1;
        
        if (remote.status !== "offline") {
          headerCount++;
          portEls[id].subLbl.textContent = remote.status.toUpperCase() + " [" + remote.type.toUpperCase() + "]";
          portEls[id].subLbl.style.fill = remote.status === "busy" ? "var(--data)" : "var(--data)";
          portEls[id].aliasLbl.textContent = remote.name;
          portEls[id].box.style.stroke = "var(--orange)";
        } else {
          portEls[id].subLbl.textContent = "OFFLINE";
          portEls[id].subLbl.style.fill = "var(--ink-low)";
          portEls[id].aliasLbl.textContent = "---";
          portEls[id].box.style.stroke = "var(--panel-line)";
          portGlow(id, false);
        }
      }

      el("header-meta").textContent = headerCount + " ports active · EP1/2 data · EP3/4 host";

      // 2. Tear down any rendered circuits that are no longer active in the backend
      for (const key of Object.keys(bridges)) {
        const [a, b] = key.split("-").map(Number);
        const portA = ports[a - 1];
        const portB = ports[b - 1];
        
        // Active and latched check: Keep the circuit visually drawn if it was connected in the backend within the last 4.5 seconds
        const backendConnected = portA && portB && 
                                 portA.status === 'busy' && 
                                 portB.status === 'busy' && 
                                 portA.connectedTo === (b - 1) && 
                                 portB.connectedTo === (a - 1);
                                 
        const recentlyConnected = portA && portB && 
                                  (Date.now() - portA.lastConnectTime < 4500) &&
                                  portA.lastConnectedTo === (b - 1);

        const visuallyActive = backendConnected || recentlyConnected;
        
        if (!visuallyActive) {
          teardownVirtualCircuit(a, b);
          portState[a] = ports[a - 1].status;
          portState[b] = ports[b - 1].status;
        }
      }

      // 3. Build any circuits that are active or recently active in the backend but not yet rendered in the frontend
      for (let i = 0; i < ports.length; i++) {
        const remote = ports[i];
        const id = remote.id + 1;
        
        const backendConnected = remote.status === "busy";
        const recentlyConnected = (Date.now() - remote.lastConnectTime < 4500) && remote.lastConnectedTo !== -1;
        
        if (backendConnected || recentlyConnected) {
          const targetId = (backendConnected ? remote.connectedTo : remote.lastConnectedTo) + 1;
          if (id < targetId) {
            const key = id + "-" + targetId;
            if (!bridges[key]) {
              buildVirtualCircuit(id, targetId);
              portState[id] = "busy";
              portState[targetId] = "busy";
            }
          }
        }
      }

      // 3b. Update existing circuits based on real-time raw data activity (EP1/EP2)
      for (const key of Object.keys(bridges)) {
        const [a, b] = key.split("-").map(Number);
        const portA = ports[a - 1];
        const portB = ports[b - 1];
        if (portA && portB) {
          const bridge = bridges[key];
          const dataActive = portA.dataActive || portB.dataActive;
          
          const strokeColor = dataActive ? "var(--data)" : "#1c3d27"; // active bright green vs connected but idle dark pine-green
          const strokeWidth = dataActive ? "2.6" : "1.5";
          
          if (bridge.lane && bridge.lane.path) {
            bridge.lane.path.setAttribute("stroke", strokeColor);
            bridge.lane.path.setAttribute("stroke-width", strokeWidth);
          }
          
          if (bridge.dots) {
            bridge.dots.forEach(d => {
              // Hide/show the flowing particles dynamically based on active byte transfers
              d.dot.style.opacity = dataActive ? "1" : "0";
            });
          }

          if (dataActive) {
            flashPad(a, 1, "var(--data)");
            flashPad(a, 2, "var(--data)");
            flashPad(b, 1, "var(--data)");
            flashPad(b, 2, "var(--data)");
          }
        }
      }

      // 4. Trace control plane events (MSG_SESSION 0x09) to visually animate packet flows on the control plane
      if (controlEvents && controlEvents.length > 0) {
        if (!window.processedControlEvents) {
          window.processedControlEvents = new Set();
        }
        
        controlEvents.forEach(evt => {
          if (window.processedControlEvents.has(evt.id)) return;
          window.processedControlEvents.add(evt.id);
          
          // Ignore old historical events on first fetch / tab reactivation
          if (Date.now() - evt.timestamp > 4000) return;
          
          const sender = evt.sender;
          
          // Animate the packet flow from Port A -> Request Point -> Port B, C, D
          (async () => {
            const labelMsg = evt.isAnnounce ? "MSG_SESSION (announce)" : "MSG_SESSION";
            say("Host writes <b>" + labelMsg + "</b> on <b>Port " + sender + "</b> <b>EP4</b>...", "var(--host)");
            flashPad(sender, 4, "var(--host)");
            
            // Pulse violet dot up from host to physical port, then to Request Point (central control logic)
            await pulseAlong(hostReadD(sender), {color: "var(--host)", dur: 320});
            await pulseAlong(reqD(sender), {color: "var(--host)", dur: 420});
            boxPulse(rpGlow, "var(--host)");
            
            // Glow the port box border briefly to show session activity
            const originalBorderColor = portEls[sender].box.style.stroke;
            portEls[sender].box.style.stroke = "var(--host)";
            setTimeout(() => {
              if (ports[sender - 1] && ports[sender - 1].status !== 'offline') {
                portEls[sender].box.style.stroke = originalBorderColor;
              }
            }, 600);

            // Forward session broadcast down to all other active ports!
            for (let i = 0; i < ports.length; i++) {
              const remote = ports[i];
              const remoteId = remote.id + 1;
              if (remote.status !== "offline" && remoteId !== sender) {
                // Fire-and-forget individual branch delivery in parallel
                (async () => {
                  say("Request Point routes broadcast to <b>Port " + remoteId + "</b>.", "var(--host)");
                  await pulseAlong(reqD(remoteId), {color: "var(--host)", dur: 420, reverse: true});
                  flashPad(remoteId, 3, "var(--host)");
                  await pulseAlong(hostReplyD(remoteId), {color: "var(--host)", dur: 320});
                })().catch(err => console.error("Control plane broadcast route error:", err));
              }
            }
          })().catch(err => console.error("Control plane animation error:", err));
        });
      }

      // Keep local status synchronized
      for (let i = 0; i < ports.length; i++) {
        const remote = ports[i];
        const id = remote.id + 1;
        portState[id] = remote.status;
        portNames[id] = remote.name;
      }
    }

    function buildVirtualCircuit(a, b) {
      const key = a + "-" + b;
      bridges[key] = { status: "building" };

      // Instantly render elements to make it fully synchronous and prevent freezes
      const lane = drawData(a, b);
      const dots = [...flowDots(lane, "var(--data)", 4, false), ...flowDots(lane, "#68d391", 4, true)];
      
      // Hide dots by default on initial build, they will show when data flows
      dots.forEach(d => d.dot.style.opacity = "0");
      
      bridges[key] = { lane, dots, ports: [a, b] };

      // Play the full hardware handshake animation sequence as an asynchronous background task
      (async () => {
        say("Host writes control packet on <b>Port " + a + "</b> <b>EP4</b>...", "var(--host)");
        flashPad(a, 4, "var(--host)");
        await pulseAlong(hostReadD(a), {color: "var(--host)", dur: 380});
        await wait(100);

        say("<b>Port " + a + "</b> sends matrix crossbar connection request up to <b>Request Point</b>.", "var(--req)");
        await pulseAlong(reqD(a), {color: "var(--req)", dur: 500});
        boxPulse(rpGlow, "var(--req)");
        await wait(160);

        say("The Request Point <b>CTRL</b>s the switching fabric to wire <b>Port " + a + "</b> and <b>Port " + b + "</b>.", "var(--ctrl)");
        await pulseAlong(ctrlD(), {color: "var(--ctrl)", dur: 500});
        boxPulse(fabGlow, "var(--data)");
        await wait(120);

        say("Request Point returns <b>ACK</b> response to <b>Port " + a + "</b>.", "var(--ctrl)");
        await pulseAlong(reqD(a), {color: "var(--ctrl)", dur: 480, reverse: true});
        portGlow(a, true); portGlow(b, true);
        await wait(110);

        say("<b>Port " + a + "</b> returns ACK to host on <b>EP3</b>. File transfer begins!", "var(--host)");
        flashPad(a, 3, "var(--host)");
        await pulseAlong(hostReplyD(a), {color: "var(--host)", dur: 380});
        await wait(110);

        say("Dynamic hardware path built between <b>Port " + a + "</b> ↔ <b>Port " + b + "</b>.", "var(--data)");
      })().catch(err => console.error("Visual FX Error:", err));
    }

    function teardownVirtualCircuit(a, b) {
      const key = bridges[a + "-" + b] ? a + "-" + b : b + "-" + a;
      const br = bridges[key];
      if (!br) return;

      // Instantly teardown visual elements synchronously to prevent any freeze/wedges
      if (br.dots) {
        br.dots.forEach(d => {
          try { d.a.cancel(); } catch(e) {}
          try { d.dot.remove(); } catch(e) {}
        });
      }
      if (br.lane && br.lane.path) {
        try { br.lane.path.remove(); } catch(e) {}
      }
      
      if (br.ports) {
        br.ports.forEach(id => {
          portGlow(id, false);
        });
      }
      
      delete bridges[key];
      say("Idle. EP3/EP4 are internal to each port's host. EP1/EP2 face the crossport fabric.", "var(--green)");
    }

    // --- JSON polling ---
    let lastLogLength = 0;
    
    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) return;
        const data = await res.json();
        
        syncHardwareState(data.ports, data.logHistory, data.controlEvents);
        renderLogs(data.logHistory);
      } catch (err) {
        console.error('Error fetching status:', err);
      }
    }

    function renderLogs(logs) {
      const consoleEl = document.getElementById('log-console');
      const shouldScroll = consoleEl.scrollHeight - consoleEl.scrollTop === consoleEl.clientHeight;
      
      consoleEl.innerHTML = logs.map(line => {
        const match = line.match(/^\\\\\\[(.+?)\\\\\\] (.*)$/);
        if (!match) return '<div class="log-entry"><span class="log-text">' + line + '</span></div>';
        
        const time = match[1];
        const text = match[2];
        let textClass = 'log-text';
        
        if (text.includes('Successfully Attached') || text.includes('listening')) {
          textClass += ' success';
        } else if (text.includes('Circuit Connected') || text.includes('Connected:')) {
          textClass += ' highlight';
        } else if (text.includes('Error') || text.includes('failed') || text.includes('Refused')) {
          textClass += ' error';
        } else if (text.includes('MSG type: 0x9') || text.includes('MSG_SESSION')) {
          textClass += ' session';
        } else if (text.includes('Data Routed') || text.includes('Data packet')) {
          textClass += ' data-route';
        }
        
        return \'<div class="log-entry"><span class="log-time">[&nbsp;\' + time + \'&nbsp;]</span><span class="\' + textClass + \'">\' + text + \'</span></div>\';
      }).join('');
      
      if (logs.length > lastLogLength || shouldScroll) {
        consoleEl.scrollTop = consoleEl.scrollHeight;
      }
      lastLogLength = logs.length;
    }

    window.resetDevice = async function() {
      if (!confirm('Gracefully disconnect all active clients and restart the simulated RocketBox switching matrix?')) {
        return;
      }
      try {
        const res = await fetch('/api/reset', { method: 'POST' });
        if (res.ok) {
          for (const k of Object.keys(bridges)) {
            const [a, b] = k.split("-");
            await teardownVirtualCircuit(+a, +b);
          }
          fetchStatus();
        }
      } catch (err) {
        alert('Error resetting device: ' + err.message);
      }
    }

    setInterval(fetchStatus, 300);
    fetchStatus();

    track(rpGlow.animate([{opacity:.04},{opacity:.15},{opacity:.04}],{duration:3200,iterations:Infinity,easing:"ease-in-out"}));
  })();
</script>
</body>
</html>`;
}

const httpServer = createHttpServer((req, res) => {
  const urlObj = new URL(req.url, `http://${req.headers.host || 'localhost'}`);
  
  if (req.method === 'GET' && urlObj.pathname === '/') {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end(getHtmlDashboard());
  } else if (req.method === 'GET' && urlObj.pathname === '/api/status') {
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({
      ports: ports.map(p => ({
        id: p.id,
        status: p.status,
        name: p.name,
        systemId: p.systemId,
        type: p.type,
        connectedTo: p.connectedTo,
        dataActive: (Date.now() - p.lastDataTime) < 2200, // active if data transferred within last 2.2s for rich, visible persistence
        lastConnectTime: p.lastConnectTime,
        lastConnectedTo: p.lastConnectedTo,
      })),
      logHistory: logHistory.slice(-50),
      controlEvents: controlEvents.slice(-10), // return last 10 control events to prevent bloating
    }));
  } else if (req.method === 'POST' && urlObj.pathname === '/api/reset') {
    logEvent('SIMULATOR RESET REQUESTED VIA WEB CONTROL PANEL');
    ports.forEach(p => {
      if (p.socket) {
        try {
          if (p.type === 'ws') {
            p.socket.close(1012, 'Simulator Reset');
          } else if (p.type === 'tcp') {
            p.socket.destroy();
          }
        } catch (e) {
          console.error('Error closing socket during reset:', e);
        }
      }
      p.reset();
    });
    broadcastSystems();
    res.writeHead(200, { 'Content-Type': 'application/json' });
    res.end(JSON.stringify({ success: true }));
  } else {
    res.writeHead(404, { 'Content-Type': 'text/plain' });
    res.end('Not Found');
  }
});

httpServer.on('upgrade', (req, socket, head) => {
  const headers = req.headers;
  if (headers.upgrade && headers.upgrade.toLowerCase() === 'websocket') {
    wsServer.handleUpgrade(req, socket, head, (ws) => {
      wsServer.emit('connection', ws, req);
    });
  } else {
    socket.destroy();
  }
});

httpServer.listen(WS_PORT, LISTEN_HOST, () => {
  logEvent(`WebSocket Server & Web Dashboard listening on ${LISTEN_HOST}:${WS_PORT}`);
});

wsServer.on('connection', (ws, req) => {
  const params = new URLSearchParams(req.url.split('?')[1] || '');
  let portIndex = parseInt(params.get('port') || '-1', 10);
  if (portIndex >= 1 && portIndex <= 4) {
    portIndex = portIndex - 1;
  } else {
    portIndex = ports.findIndex(p => p.status === 'offline');
  }

  if (portIndex >= 0 && portIndex < 4) {
    const existingPort = ports[portIndex];
    if (existingPort.status !== 'offline') {
      logEvent(`Port ${portIndex + 1} occupied during connect - force-closing existing connection to allow takeover`);
      if (existingPort.socket) {
        try {
          if (existingPort.type === 'ws') {
            existingPort.socket.close(4000, 'Session Overridden');
          } else if (existingPort.type === 'tcp') {
            existingPort.socket.destroy();
          }
        } catch (e) {
          logEvent(`Error force-closing old socket: ${e.message}`);
        }
      }
      existingPort.reset();
    }
  }

  if (portIndex === -1 || ports[portIndex].status !== 'offline') {
    logEvent(`Refused WS connection - port ${portIndex + 1} occupied`);
    ws.close(1013, 'Port Occupied');
    return;
  }

  const port = ports[portIndex];
  port.socket = ws;
  port.type = 'ws';
  port.status = 'reachable';
  const activePortRef = { current: port };

  logEvent(`WS Connected to Port ${port.id + 1}`);
  sendControlPacket(port, MSG_ATTACHED, 0, port.id);
  applyState1DefaultPairs();
  replayAnnouncements(port);
  broadcastSystems();

  ws.on('message', data => {
    const buf = Buffer.from(data);
    if (buf.length === 0) return;
    const epId = buf.readUInt8(0);

    if (epId === 0x04) {
      if (buf.length >= 3) {
        const verByte = buf.readUInt8(1);
        const typeByte = buf.readUInt8(2);
        if (verByte !== 0x01 || typeByte === 0x00) {
          // Mode 11 Crossbar Switch Request (16 bytes payload)
          const destPort = buf.readUInt8(1) & 0x0F;
          handleCrossbarSwitchRequest(activePortRef.current, destPort);
          return;
        }
      } else if (buf.length === 2) {
        // Disambiguate if packet is too short (unlikely for WS frame, but let's be safe)
        const verByte = buf.readUInt8(1);
        if (verByte !== 0x01) {
          const destPort = verByte & 0x0F;
          handleCrossbarSwitchRequest(activePortRef.current, destPort);
          return;
        }
      }
      if (buf.length >= 13) {
        const header = buf.subarray(1, 13);
        const payload = buf.subarray(13);
        handleControlMessage(activePortRef, header, payload);
      }
    } else if (epId === 0x01) {
      const dataPayload = buf.subarray(5); // skip EP ID (1B) + length (4B)
      handleDataPacket(activePortRef.current, dataPayload);
    }
  });

  ws.on('close', () => {
    handleClientDisconnect(activePortRef.current);
  });

  ws.on('error', err => {
    logEvent(`WS Socket Error: ${err.message}`);
    ws.close();
  });
});

// Initial draw
renderDashboard();
