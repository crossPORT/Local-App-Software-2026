import { readline } from 'node:process';
import { createInterface } from 'node:readline';
// Resolve relative import to TypeScript SDK
import { RocketBox, SimTransport } from '../../sdks/typescript/src/index.ts';

const portArg = parseInt(process.argv[2] || '1', 10);
if (portArg < 1 || portArg > 4) {
  console.error('Usage: node interactive-client.js <port-number-1-to-4>');
  process.exit(1);
}

const rl = createInterface({
  input: process.stdin,
  output: process.stdout,
  prompt: `[Port ${portArg}]> `
});

console.log(`Connecting to Simulated Hardware on Port ${portArg}...`);

try {
  // 1. Attach to the simulated Port using the SDK
  const transport = new SimTransport(portArg);
  const session = await RocketBox.attach(transport, portArg);

  console.log(`Attached successfully!`);
  console.log(`Your System ID: \x1b[1m\x1b[32m${session.systemId}\x1b[reset\x1b[0m`);
  console.log('\nAvailable commands:');
  console.log('  \x1b[1m\x1b[36mlist\x1b[0m                 - List all reachable ports');
  console.log('  \x1b[1m\x1b[36mconnect <system-id>\x1b[0m  - Connect to another port (e.g. connect sys-port-2)');
  console.log('  \x1b[1m\x1b[36msend <message>\x1b[0m       - Send data payload to connected peer');
  console.log('  \x1b[1m\x1b[36mdisconnect\x1b[0m           - Cleanly close current active circuit');
  console.log('  \x1b[1m\x1b[36mexit\x1b[0m                 - Detach and close client\n');

  let activeConnection = null;

  // Set up background callbacks on the Session
  session.onSystemsChanged((systems) => {
    console.log(`\n\x1b[33m[Update] systems list changed:\x1b[0m`);
    systems.forEach(s => {
      console.log(`  - ${s.id} (${s.name}): ${s.status}`);
    });
    rl.prompt();
  });

  session.onIncomingCircuit((conn) => {
    console.log(`\n\x1b[1m\x1b[32m[Circuit Up] Incoming connection established from: ${conn.peerSystemId}\x1b[0m`);
    activeConnection = conn;

    conn.onReceived((bytes) => {
      const msg = new TextDecoder().decode(bytes);
      console.log(`\n\x1b[1m\x1b[34m[Received Payload] ${conn.peerSystemId}: "${msg}"\x1b[0m`);
      rl.prompt();
    });

    conn.onClosed((reason) => {
      console.log(`\n\x1b[1m\x1b[31m[Circuit Down] Connection closed, reason: ${reason}\x1b[0m`);
      activeConnection = null;
      rl.prompt();
    });

    rl.prompt();
  });

  rl.prompt();

  rl.on('line', async (line) => {
    const input = line.trim();
    if (input === 'exit') {
      rl.close();
      return;
    }

    const [cmd, ...args] = input.split(' ');

    try {
      switch (cmd) {
        case 'list': {
          const systems = await session.listSystems();
          console.log(`Reachable Systems (${systems.length}):`);
          systems.forEach(s => {
            console.log(`  - ${s.id} (${s.name}): ${s.status}`);
          });
          break;
        }

        case 'connect': {
          const target = args[0];
          if (!target) {
            console.error('Error: connect requires target system ID (e.g. connect sys-port-2)');
            break;
          }
          console.log(`Connecting to ${target}...`);
          activeConnection = await session.connect(target);
          console.log(`\x1b[1m\x1b[32mCircuit successfully established with ${target}!\x1b[0m`);

          activeConnection.onReceived((bytes) => {
            const msg = new TextDecoder().decode(bytes);
            console.log(`\n\x1b[1m\x1b[34m[Received Payload] ${activeConnection.peerSystemId}: "${msg}"\x1b[0m`);
            rl.prompt();
          });

          activeConnection.onClosed((reason) => {
            console.log(`\n\x1b[1m\x1b[31m[Circuit Down] Connection closed, reason: ${reason}\x1b[0m`);
            activeConnection = null;
            rl.prompt();
          });
          break;
        }

        case 'send': {
          if (!activeConnection) {
            console.error('Error: No active circuit. Use connect first.');
            break;
          }
          const msg = args.join(' ');
          if (!msg) {
            console.error('Error: send requires a message payload');
            break;
          }
          const bytes = new TextEncoder().encode(msg);
          await activeConnection.send(bytes);
          console.log(`Sent: "${msg}"`);
          break;
        }

        case 'disconnect': {
          if (!activeConnection) {
            console.error('Error: No active circuit to disconnect.');
            break;
          }
          console.log('Closing connection...');
          await activeConnection.close();
          activeConnection = null;
          break;
        }

        case '':
          break;

        default:
          console.log(`Unknown command: "${cmd}"`);
          break;
      }
    } catch (err) {
      console.error(`\x1b[31mError executing command: ${err.message}\x1b[0m`);
    }

    rl.prompt();
  });

  rl.on('close', async () => {
    console.log('Detaching session and exiting...');
    if (activeConnection) {
      await activeConnection.close();
    }
    await transport.disconnect();
    process.exit(0);
  });

} catch (err) {
  console.error(`\x1b[31mFailed to run client: ${err.message}\x1b[0m`);
  process.exit(1);
}
