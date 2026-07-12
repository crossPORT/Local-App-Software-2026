export interface SystemInfo {
  id: string;
  name: string;
  status: 'reachable' | 'busy' | 'offline';
  /** Peer receive policy from announce note (HW presence). */
  receive?: 'open' | 'ask_first' | 'busy';
}
