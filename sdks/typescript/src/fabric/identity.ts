export type ReceiveStatus = 'open' | 'ask_first' | 'busy';

export interface AnnounceIdentity {
  display_name: string;
  team: string;
  receive_status: ReceiveStatus;
}
