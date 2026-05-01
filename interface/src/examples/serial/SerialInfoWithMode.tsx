import React, { FC } from 'react';
import { Box } from '@mui/material';
import { UartModeSwitcher } from '../../components';
import SerialInfo from './SerialInfo';

const SerialInfoWithMode: FC = () => {
  return (
    <Box>
      <UartModeSwitcher currentMode="reader" />
      <SerialInfo />
    </Box>
  );
};

export default SerialInfoWithMode;
