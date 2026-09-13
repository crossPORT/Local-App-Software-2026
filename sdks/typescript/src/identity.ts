export type ReceiveStatus = 'open' | 'ask_first' | 'busy';

export type AnnounceIdentity = {
  display_name: string;
  team: string;
  receive_status: ReceiveStatus;
};
