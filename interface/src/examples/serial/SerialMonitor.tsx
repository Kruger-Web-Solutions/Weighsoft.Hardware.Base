import React, { FC, useContext, useEffect, useState } from 'react';
import { Alert, Box, CircularProgress } from '@mui/material';
import { useLayoutTitle } from '../../components';
import { readUartMode } from '../../api/uartMode';
import { UartMode } from '../../types/uartMode';
import { FeaturesContext } from '../../contexts/features';
import { readWSoftStatus } from '../../project/wsoft/api';
import { WSoftSource } from '../../project/wsoft/types';
import SerialReader from './SerialReader';
import SerialWriter from '../serialwriter/SerialWriter';

const SerialMonitor: FC = () => {
  useLayoutTitle('Serial');
  const { features } = useContext(FeaturesContext);
  const [mode, setMode] = useState<UartMode | null>(null);
  // W-Soft only: where the weight comes from. The Serial reader runs only for an RS232 scale.
  const [wsoftSource, setWsoftSource] = useState<WSoftSource | null>(null);

  useEffect(() => {
    readUartMode().then((res) => setMode(res.data.mode)).catch(() => setMode('reader'));
  }, []);

  useEffect(() => {
    if (!features.wsoft) return;
    readWSoftStatus().then((res) => setWsoftSource(res.data.source)).catch(() => setWsoftSource(null));
  }, [features.wsoft]);

  if (features.wsoft && wsoftSource === 'rs485') {
    return (
      <Box p={3}>
        <Alert severity="info">
          This board reads the scale over RS485 (the weighing card), so the Serial reader is off and this page has
          nothing to set. It is only for an RS232 scale: switch the source under W-Soft &gt; Settings.
        </Alert>
      </Box>
    );
  }

  if (mode === null) return <Box p={3}><CircularProgress size={24} /></Box>;

  // A W-Soft board has no Writer service (its serial port reads the scale), so it always
  // shows the Reader pages, even if an old saved mode says writer.
  if (mode === 'writer' && !features.wsoft) return <SerialWriter />;
  return <SerialReader />;  // 'reader' OR '' (NEW) → render reader screens; the mode switcher inside surfaces the NEW prompt
};

export default SerialMonitor;
