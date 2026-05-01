import React, { FC, useEffect, useState } from 'react';
import { Box, CircularProgress } from '@mui/material';
import { useLayoutTitle } from '../../components';
import { readUartMode } from '../../api/uartMode';
import { UartMode } from '../../types/uartMode';
import SerialReader from './SerialReader';
import SerialWriter from '../serialwriter/SerialWriter';

const SerialMonitor: FC = () => {
  useLayoutTitle('Serial');
  const [mode, setMode] = useState<UartMode | null>(null);

  useEffect(() => {
    readUartMode().then((res) => setMode(res.data.mode)).catch(() => setMode('reader'));
  }, []);

  if (mode === null) return <Box p={3}><CircularProgress size={24} /></Box>;

  if (mode === 'writer') return <SerialWriter />;
  if (mode === 'diagnostics') return <Box p={3}>Diagnostics mode is active. Open the Diagnostics top-level menu to run hardware tests.</Box>;
  return <SerialReader />;  // 'reader' OR '' (NEW) → render reader screens; the mode switcher inside surfaces the NEW prompt
};

export default SerialMonitor;
