import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Box, Tab } from '@mui/material';
import { RouterTabs, useRouterTab, UartModeSwitcher } from '../../components';

import WriterStatus from './WriterStatus';
import WriterSettings from './WriterSettings';
import SendMessage from './SendMessage';
import SendViaWeb from './SendViaWeb';
import WriterLiveStream from './WriterLiveStream';
import WriterMqtt from './WriterMqtt';

const SerialWriter: FC = () => {
  const { routerTab } = useRouterTab();
  return (
    <Box>
      <UartModeSwitcher currentMode="writer" />
      <RouterTabs value={routerTab}>
        <Tab value="status"        label="Status" />
        <Tab value="settings"      label="Settings" />
        <Tab value="send-message"  label="Send Message" />
        <Tab value="send-via-web"  label="Send via Web" />
        <Tab value="live-stream"   label="Live Stream" />
        <Tab value="mqtt"          label="MQTT" />
      </RouterTabs>
      <Routes>
        <Route path="status"        element={<WriterStatus />} />
        <Route path="settings"      element={<WriterSettings />} />
        <Route path="send-message"  element={<SendMessage />} />
        <Route path="send-via-web"  element={<SendViaWeb />} />
        <Route path="live-stream"   element={<WriterLiveStream />} />
        <Route path="mqtt"          element={<WriterMqtt />} />
        <Route path="/*" element={<Navigate replace to="status" />} />
      </Routes>
    </Box>
  );
};

export default SerialWriter;
