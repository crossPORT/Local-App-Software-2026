import { describe, expect, it, beforeAll, afterAll } from 'vitest';
import { RocketBox } from '@rocketbox/sdk';
import { SimTransport } from '../../../../sdks/typescript/src/transports/sim';
import { WebSocket, WebSocketServer } from 'ws';

// Polyfill global WebSocket for SimTransport in node environment
if (typeof globalThis.WebSocket === 'undefined') {
  (globalThis as any).WebSocket = WebSocket;
}

// Protocol Constants
const MSG_ATTACH = 0x01;
const MSG_LIST = 0x02;
const MSG_CONNECT = 0x03;
const MSG_DISCONNECT = 0x04;
const MSG_ATTACHED = 0x81;
const MSG_SYSTEMS = 0x82;
const MSG_ACK = 0x83;
const MSG_CIRCUIT_UP = 0x85;
const MSG_CIRCUIT_DOWN = 0x86;

function makeControlPacket(type: number, txn: number, arg: number, payload = new Uint8Array(0)): Uint8Array {
  const buf = new Uint8Array(1 + 12 + payload.length);
  buf[0] = 0x03; // EP3 Control IN
  const view = new DataView(buf.buffer, buf.byteOffset + 1, 12);
  view.setUint8(0, 0x01); // Version
  view.setUint8(1, type);
  view.setUint16(2, txn);
  view.setUint32(4, arg);
  view.setUint32(8, payload.length);
  buf.set(payload, 1 + 12);
  return buf;
}

function buildMockSystemsPayload(activePorts: Array<{ id: number; name: string; status: 'reachable' | 'busy' | 'offline' }>): Uint8Array {
  const buffers: Uint8Array[] = [];
  const countBuf = new Uint8Array(4);
  const countView = new DataView(countBuf.buffer);
  countView.setUint32(0, activePorts.length);
  buffers.push(countBuf);

  for (const p of activePorts) {
    const idBuf = new TextEncoder().encode(`sys-port-${p.id + 1}`);
    const nameBuf = new TextEncoder().encode(p.name);
    const item = new Uint8Array(1 + idBuf.length + 1 + nameBuf.length + 1);
    
    let offset = 0;
    item[offset++] = idBuf.length;
    item.set(idBuf, offset);
    offset += idBuf.length;
    
    item[offset++] = nameBuf.length;
    item.set(nameBuf, offset);
    offset += nameBuf.length;
    
    const statusVal = p.status === 'busy' ? 2 : p.status === 'offline' ? 3 : 1;
    item[offset] = statusVal;
    
    buffers.push(item);
  }

  const totalLen = buffers.reduce((acc, b) => acc + b.length, 0);
  const result = new Uint8Array(totalLen);
  let pos = 0;
  for (const b of buffers) {
    result.set(b, pos);
    pos += b.length;
  }
  return result;
}

describe('TypeScript SDK Integration with Simulated Hardware', () => {
  let wss: WebSocketServer;
  const mockPorts: Array<{
    id: number;
    name: string;
    status: 'reachable' | 'busy' | 'offline';
    socket: WebSocket | null;
    connectedTo: number;
  }> = Array.from({ length: 4 }, (_, i) => ({
    id: i,
    name: `Port ${i + 1}`,
    status: 'offline',
    socket: null,
    connectedTo: -1,
  }));

  beforeAll(async () => {
    // Spin up an in-process, independent mock WebSocket server on Port 1774
    wss = new WebSocketServer({ port: 1774 });

    wss.on('connection', (ws, req) => {
      const params = new URLSearchParams(req.url?.split('?')[1] || '');
      let portNum = parseInt(params.get('port') || '-1', 10);
      if (portNum < 1 || portNum > 4) {
        portNum = mockPorts.findIndex(p => p.status === 'offline') + 1;
      }
      
      const portIdx = portNum - 1;
      const port = mockPorts[portIdx];
      port.socket = ws;
      port.status = 'reachable';

      ws.on('message', (data) => {
        const buf = new Uint8Array(data as ArrayBuffer);
        if (buf.length === 0) return;
        const epId = buf[0];

        if (epId === 0x04) {
          // EP4 Control OUT
          if (buf.length < 13) return;
          const view = new DataView(buf.buffer, buf.byteOffset + 1, 12);
          const type = view.getUint8(1);
          const txn = view.getUint16(2);
          const arg = view.getUint32(4);

          switch (type) {
            case MSG_ATTACH: {
              ws.send(makeControlPacket(MSG_ATTACHED, txn, port.id));
              break;
            }
            case MSG_LIST: {
              const activeList = mockPorts.filter(p => p.status !== 'offline');
              ws.send(makeControlPacket(MSG_SYSTEMS, txn, 0, buildMockSystemsPayload(activeList)));
              break;
            }
            case MSG_CONNECT: {
              // Parse target systemId from payload
              const targetSysId = new TextDecoder().decode(buf.subarray(13));
              const targetPortNum = parseInt(targetSysId.replace('sys-port-', ''), 10);
              const targetIdx = targetPortNum - 1;
              const targetPort = mockPorts[targetIdx];

              if (targetPort && targetPort.status === 'reachable') {
                port.status = 'busy';
                port.connectedTo = targetIdx;
                targetPort.status = 'busy';
                targetPort.connectedTo = portIdx;

                // Send MSG_CIRCUIT_UP to receiver
                targetPort.socket?.send(makeControlPacket(MSG_CIRCUIT_UP, 0, 0, new TextEncoder().encode(`sys-port-${portIdx + 1}`)));
                // ACK sender
                ws.send(makeControlPacket(MSG_ACK, txn, 0));
              }
              break;
            }
            case MSG_DISCONNECT: {
              if (port.connectedTo !== -1) {
                const targetPort = mockPorts[port.connectedTo];
                port.status = 'reachable';
                port.connectedTo = -1;
                
                if (targetPort) {
                  targetPort.status = 'reachable';
                  targetPort.connectedTo = -1;
                  // Notify target that connection broke
                  targetPort.socket?.send(makeControlPacket(MSG_CIRCUIT_DOWN, 0, 0));
                }
              }
              ws.send(makeControlPacket(MSG_ACK, txn, 0));
              break;
            }
          }
        } else if (epId === 0x01) {
          // EP1 Data OUT (Route to receiver EP2)
          if (port.status === 'busy' && port.connectedTo !== -1) {
            const targetPort = mockPorts[port.connectedTo];
            if (targetPort && targetPort.socket) {
              const forwardPacket = new Uint8Array(buf.length);
              forwardPacket.set(buf);
              forwardPacket[0] = 0x02; // Change to EP2 Data IN
              targetPort.socket.send(forwardPacket);
            }
          }
        }
      });

      ws.on('close', () => {
        port.status = 'offline';
        port.socket = null;
        if (port.connectedTo !== -1) {
          const targetPort = mockPorts[port.connectedTo];
          port.connectedTo = -1;
          if (targetPort) {
            targetPort.status = 'reachable';
            targetPort.connectedTo = -1;
            targetPort.socket?.send(makeControlPacket(MSG_CIRCUIT_DOWN, 0, 0));
          }
        }
      });
    });
  });

  afterAll(async () => {
    // Gracefully shut down the mock server
    await new Promise<void>((resolve) => {
      wss.close(() => resolve());
    });
  });

  it('connects two sessions, list systems, connects them, and transfers data', async () => {
    // 1. Attach Session 1 on Port 3 (using Port 1774 mock server)
    const transport1 = new SimTransport(3, 1774);
    const session1 = await RocketBox.attach(transport1, 3);
    expect(session1.systemId).toBe('sys-port-3');

    // 2. Attach Session 2 on Port 4 (using Port 1774 mock server)
    const transport2 = new SimTransport(4, 1774);
    const session2 = await RocketBox.attach(transport2, 4);
    expect(session2.systemId).toBe('sys-port-4');

    // Give mock server and clients a moment to sync system changes
    await new Promise(resolve => setTimeout(resolve, 100));

    // 3. Query systems list from Session 1
    const systems = await session1.listSystems();
    expect(systems.length).toBeGreaterThanOrEqual(1);

    const peer = systems.find(s => s.id === 'sys-port-4');
    expect(peer).toBeDefined();
    expect(peer!.status).toBe('reachable');

    // 4. Set up incoming circuit handling and data receive handlers on Session 2
    let incomingTriggered = false;
    let receivedBytes: Uint8Array | null = null;
    let receivedMessage: Uint8Array | null = null;

    session2.onIncomingCircuit((conn) => {
      incomingTriggered = true;
      conn.onReceived((bytes) => {
        receivedBytes = bytes;
      });
      conn.onMessageReceived((msg) => {
        receivedMessage = msg;
      });
    });

    // 5. Establish circuit from Session 1 -> Session 4
    const conn1 = await session1.connect('sys-port-4');
    expect(conn1).toBeDefined();
    expect(conn1.state).toBe('open');

    // Wait for the incoming circuit handler to fire on Session 2
    for (let i = 0; i < 20; i++) {
      if (incomingTriggered) break;
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    expect(incomingTriggered).toBe(true);

    // 6. Test Stream Data Transfer (EP1 -> EP2)
    const testPayload = new Uint8Array([72, 101, 108, 108, 111, 32, 83, 68, 75, 33]); // "Hello SDK!"
    await conn1.send(testPayload);

    // Wait for bytes to be received
    for (let i = 0; i < 20; i++) {
      if (receivedBytes) break;
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    expect(receivedBytes).not.toBeNull();
    expect(Array.from(receivedBytes!)).toEqual(Array.from(testPayload));

    // 7. Test Message-Framed Data Transfer (optional message layer)
    const testMessage = new Uint8Array([84, 83, 32, 83, 68, 75]); // "TS SDK"
    await conn1.sendMessage(testMessage);

    // Wait for message to be received and assembled
    for (let i = 0; i < 20; i++) {
      if (receivedMessage) break;
      await new Promise(resolve => setTimeout(resolve, 50));
    }
    expect(receivedMessage).not.toBeNull();
    expect(Array.from(receivedMessage!)).toEqual(Array.from(testMessage));

    // 8. Close Connection and clean up
    await conn1.close();
    expect(conn1.state).toBe('closed');

    await transport1.disconnect();
    await transport2.disconnect();
  });
});
