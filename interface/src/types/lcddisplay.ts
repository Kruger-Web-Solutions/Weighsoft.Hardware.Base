export interface LcdDisplayData {
  line1: string;
  line2: string;
  i2c_address: number;
  backlight: boolean;
  
  // Serial bridge fields
  bridge_mode: 'off' | 'websocket' | 'mqtt' | 'ble';
  serial_device_ip: string;
  serial_device_port: number;
  serial_mqtt_topic: string;
  serial_ble_service_uuid: string;
  serial_ble_char_uuid: string;
}
