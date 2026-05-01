export interface KnownWriter {
  id: string;
  friendly_name: string;
  ip: string;
  first_seen_at: number;
  last_seen_at: number;
  last_message_at: number;
  last_message: string;
  online: boolean;
}

export interface KnownWritersData {
  writers: KnownWriter[];
}
