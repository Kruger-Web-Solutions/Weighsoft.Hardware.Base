import React, { FC, useContext, useEffect, useState } from 'react';
import { Box, CircularProgress } from '@mui/material';
import { useLayoutTitle } from '../../components';
import { readUartMode } from '../../api/uartMode';
import { UartMode } from '../../types/uartMode';
import { FeaturesContext } from '../../contexts/features';
import SerialReader from './SerialReader';
import SerialWriter from '../serialwriter/SerialWriter';

const SerialMonitor: FC = () => {
  useLayoutTitle('Serial');
  const { features } = useContext(FeaturesContext);
  const [mode, setMode] = useState<UartMode | null>(null);

  useEffect(() => {
    readUartMode().then((res) => setMode(res.data.mode)).catch(() => setMode('reader'));
  }, []);

  if (mode === null) return <Box p={3}><CircularProgress size={24} /></Box>;

  // A W-Soft board has no Writer service (its serial port reads the scale), so it always
  // shows the Reader pages, even if an old saved mode says writer.
  if (mode === 'writer' && !features.wsoft) return <SerialWriter />;
  return <SerialReader />;  // 'reader' OR '' (NEW) → render reader screens; the mode switcher inside surfaces the NEW prompt
};

export default SerialMonitor;
