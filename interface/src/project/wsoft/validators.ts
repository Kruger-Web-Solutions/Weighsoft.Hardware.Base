import Schema, { InternalRuleItem } from 'async-validator';

// Same rules as lib/wsoft_core/src/wsoft_settings.cpp, so the form catches what the board rejects.
const TOPIC_MAX = 96;
const ID_MAX = 48;

const isValidTopic = (value: string) => {
  if (!value || value.length > TOPIC_MAX) return false;
  if (value.startsWith('/') || value.endsWith('/')) return false;
  if (!/^[\x21-\x7e]+$/.test(value) || value.includes('+')) return false;
  return !/#(?!\{)/.test(value);
};

const TOPIC_VALIDATOR = {
  validator(rule: InternalRuleItem, value: string, callback: (error?: string) => void) {
    if (!isValidTopic(value)) {
      callback(`Up to ${TOPIC_MAX} characters, no spaces or '+', no '/' at either end; '#' only as #{...}`);
    } else {
      callback();
    }
  }
};

export const WSOFT_SETTINGS_VALIDATOR = new Schema({
  publish_interval_ms: [
    { required: true, message: 'Publish interval is required' },
    { type: 'number', min: 250, max: 60000, message: 'Publish interval must be 250 to 60000 ms' }
  ],
  mqtt_base_path: [
    { required: true, message: 'MQTT base path is required' },
    TOPIC_VALIDATOR
  ],
  mqtt_name: [
    { required: true, message: 'Name is required' },
    {
      pattern: new RegExp(`^[\\x21-\\x7e](?:[\\x20-\\x7e]{0,${ID_MAX - 2}}[\\x21-\\x7e])?$`),
      message: `Up to ${ID_MAX} characters, no space at either end`
    }
  ],
  mqtt_unique_id: [
    { required: true, message: 'Unique id is required' },
    { pattern: new RegExp(`^[\\x21-\\x7e]{1,${ID_MAX}}$`), message: `Up to ${ID_MAX} characters, no spaces` }
  ]
});
