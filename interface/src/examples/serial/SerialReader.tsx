import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { RouterTabs, useRouterTab } from '../../components';

import SerialInfoWithMode from './SerialInfoWithMode';
import SerialConfig from './SerialConfig';
import SerialRest from './SerialRest';
import SerialWebSocket from './SerialWebSocket';
import SerialBle from './SerialBle';
import Writers from './Writers';

const SerialReader: FC = () => {
  const { routerTab } = useRouterTab();
  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="information" label="Information" />
        <Tab value="configuration" label="Configuration" />
        <Tab value="rest" label="REST View" />
        <Tab value="stream" label="Live Stream" />
        <Tab value="ble" label="BLE Stream" />
        <Tab value="writers" label="Writers" />
      </RouterTabs>
      <Routes>
        <Route path="information" element={<SerialInfoWithMode />} />
        <Route path="configuration" element={<SerialConfig />} />
        <Route path="rest" element={<SerialRest />} />
        <Route path="stream" element={<SerialWebSocket />} />
        <Route path="ble" element={<SerialBle />} />
        <Route path="writers" element={<Writers />} />
        <Route path="/*" element={<Navigate replace to="information" />} />
      </Routes>
    </>
  );
};

export default SerialReader;
