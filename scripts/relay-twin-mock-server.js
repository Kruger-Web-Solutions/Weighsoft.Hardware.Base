/**
 * Local mock API so the React UI can load the Relay Board twin without a live ESP.
 * Usage: node scripts/relay-twin-mock-server.js
 * Then:  set PROXY=http://127.0.0.1:3080 && npm start (in interface/)
 */
const http = require('http');

const PORT = 3080;

const b64url = (obj) =>
  Buffer.from(JSON.stringify(obj))
    .toString('base64')
    .replace(/=/g, '')
    .replace(/\+/g, '-')
    .replace(/\//g, '_');

const fakeJwt = () =>
  `${b64url({ alg: 'HS256', typ: 'JWT' })}.${b64url({
    username: 'admin',
    admin: true
  })}.mock`;

let relays = {
  relay1: false,
  relay2: false,
  relay3: false,
  relay4: false,
  buzzer: false
};

const json = (res, code, body) => {
  const data = JSON.stringify(body);
  res.writeHead(code, {
    'Content-Type': 'application/json',
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Headers': 'Authorization, Content-Type',
    'Access-Control-Allow-Methods': 'GET,POST,OPTIONS'
  });
  res.end(data);
};

const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (req.method === 'OPTIONS') {
    return json(res, 204, {});
  }

  if (url === '/rest/features' && req.method === 'GET') {
    return json(res, 200, {
      project: true,
      security: true,
      mqtt: true,
      ntp: true,
      ota: true,
      upload_firmware: true,
      ble: false
    });
  }

  if (url === '/rest/signIn' && req.method === 'POST') {
    let body = '';
    req.on('data', (c) => (body += c));
    req.on('end', () => {
      json(res, 200, { access_token: fakeJwt() });
    });
    return;
  }

  if (url === '/rest/verifyAuthorization' && req.method === 'GET') {
    return json(res, 200, {});
  }

  if (url === '/rest/relayBoard' && req.method === 'GET') {
    return json(res, 200, relays);
  }

  if (url === '/rest/relayBoard' && req.method === 'POST') {
    let body = '';
    req.on('data', (c) => (body += c));
    req.on('end', () => {
      try {
        relays = { ...relays, ...JSON.parse(body || '{}') };
      } catch (_) {
        /* keep */
      }
      json(res, 200, relays);
    });
    return;
  }

  if (url === '/rest/relayBoardStatus' && req.method === 'GET') {
    return json(res, 200, {
      board: 'RelayBoardEspBuildIn',
      mcu: 'ESP-12F',
      platform: 'esp8266',
      chip_id: 'mocka1',
      cpu_freq_mhz: 160,
      free_heap: 37248,
      heap_fragmentation: 11,
      flash_chip_size: 4194304,
      sketch_size: 854040,
      free_sketch_space: 1500000,
      sdk_version: 'mock',
      has_temp_sensor: false,
      has_buzzer: false,
      power_led: 'hardwired',
      relay_active: 'low',
      pins: { ry1: 16, ry2: 14, ry3: 12, ry4: 13 },
      gpio_legend: {
        '16': 'DO RY1',
        '14': 'DO RY2',
        '12': 'DO RY3',
        '13': 'DO RY4',
        '4': 'DI/DO breakout',
        '5': 'DI/DO breakout',
        '0': 'BOOT strap / flash',
        '2': 'BOOT strap',
        '15': 'BOOT strap',
        '1': 'TX0 UART',
        '3': 'RX0 UART'
      },
      relays
    });
  }

  if (url === '/rest/systemStatus' && req.method === 'GET') {
    return json(res, 200, {
      esp_platform: 'esp8266',
      max_alloc_heap: 30000,
      cpu_freq_mhz: 160,
      free_heap: 37248,
      sketch_size: 854040,
      free_sketch_space: 1500000,
      sdk_version: 'mock',
      flash_chip_size: 4194304,
      flash_chip_speed: 40000000,
      fs_used: 12000,
      fs_total: 1000000,
      heap_fragmentation: 11
    });
  }

  json(res, 404, { error: 'not found', path: url });
});

server.listen(PORT, '127.0.0.1', () => {
  console.log(`Relay twin mock API on http://127.0.0.1:${PORT}`);
});
