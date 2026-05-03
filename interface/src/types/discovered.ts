// Mirror of MdnsBrowser::DiscoveredDevice on the firmware side. Returned by
// GET /rest/discovered as `{ devices: [...] }`.
export interface DiscoveredDevice {
  id: string;            // Hardware unique ID (TXT record `id`)
  name: string;          // Friendly name (TXT record `name`, falls back to hostname)
  mode: '' | 'reader' | 'writer' | 'diagnostics' | 'new';
  hostname: string;      // mDNS hostname e.g. weighsoft-abc123.local
  ip: string;            // Dotted-quad IPv4
  port: number;
  last_seen_ms_ago: number;
}

export interface DiscoveredDevicesData {
  devices: DiscoveredDevice[];
}
