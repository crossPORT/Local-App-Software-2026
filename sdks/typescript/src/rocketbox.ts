import type { Transport } from './transport';
import { Session } from './session_core';

export class RocketBox {
  static async attach(transport: Transport, port: number = 1): Promise<Session> {
    const session = new Session(transport, port);
    await session.init();
    return session;
  }
}
