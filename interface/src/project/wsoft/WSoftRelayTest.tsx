import { FC, useCallback, useEffect, useState } from 'react';

import { Button, Chip, Stack, Typography } from '@mui/material';

import { MessageBox, SectionContent } from '../../components';
import { extractErrorMessage } from '../../utils';

import * as WSoftApi from './api';
import { WSoftRelayBoard, WSoftRelayStatus } from './types';

const RELAYS: { key: keyof WSoftRelayBoard; pin: 'ry1' | 'ry2' | 'ry3' | 'ry4'; label: string }[] = [
  { key: 'relay1', pin: 'ry1', label: 'Relay 1' },
  { key: 'relay2', pin: 'ry2', label: 'Relay 2' },
  { key: 'relay3', pin: 'ry3', label: 'Relay 3' },
  { key: 'relay4', pin: 'ry4', label: 'Relay 4' }
];

// The bench click test: with the Hi-Lo switch off, each fitted relay can be switched by hand.
// With the switch on, the board drives the Hi-Lo relays itself and this only shows them.
const WSoftRelayTest: FC<{ hiloOn: boolean }> = ({ hiloOn }) => {
  const [status, setStatus] = useState<WSoftRelayStatus>();
  const [errorMessage, setErrorMessage] = useState<string>();

  const load = useCallback(async () => {
    try {
      setStatus((await WSoftApi.readWSoftRelayStatus()).data);
      setErrorMessage(undefined);
    } catch (error: any) {
      setErrorMessage(extractErrorMessage(error, 'Could not read the relays'));
    }
  }, []);

  useEffect(() => {
    load();
    const timer = setInterval(load, 1000);
    return () => clearInterval(timer);
  }, [load]);

  const toggle = async (key: keyof WSoftRelayBoard) => {
    if (!status) return;
    try {
      await WSoftApi.updateWSoftRelays({ [key]: !status.relays[key] });
      await load();
    } catch (error: any) {
      setErrorMessage(extractErrorMessage(error, 'Could not switch the relay'));
    }
  };

  const driven = status?.hilo.enabled ?? hiloOn;

  return (
    <SectionContent title="Relay test" titleGutter>
      {errorMessage && <MessageBox level="error" message={errorMessage} my={1} />}
      <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
        {driven
          ? `Hi-Lo is driving the relays now (zone ${status?.hilo.zone ?? '...'}). Switch Hi-Lo relays off above to test by hand.`
          : 'Tap a relay to switch it. Listen for the click and check the light.'}
      </Typography>
      <Stack direction="row" sx={{ flexWrap: 'wrap', gap: 1 }}>
        {status && RELAYS.filter((relay) => status.pins[relay.pin] >= 0).map((relay) => (
          <Button
            key={relay.key}
            variant={status.relays[relay.key] ? 'contained' : 'outlined'}
            color={status.relays[relay.key] ? 'success' : 'primary'}
            disabled={driven}
            onClick={() => toggle(relay.key)}
          >
            {relay.label}: {status.relays[relay.key] ? 'ON' : 'OFF'}
          </Button>
        ))}
        {status && <Chip label={`ON = pin ${status.relay_active.toUpperCase()}`} variant="outlined" />}
      </Stack>
    </SectionContent>
  );
};

export default WSoftRelayTest;
