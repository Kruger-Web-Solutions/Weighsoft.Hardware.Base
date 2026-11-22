import React, { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { RouterTabs, useRouterTab } from '../../components';
import { FeaturesContext } from '../../contexts/features';

import SerialInfoWithMode from './SerialInfoWithMode';
import SerialConfig from './SerialConfig';
import SerialRest from './SerialRest';
import SerialWebSocket from './SerialWebSocket';
import SerialBle from './SerialBle';
import Writers from './Writers';

const SerialReader: FC = () => {
  const { routerTab } = useRouterTab();
  const { features } = useContext(FeaturesContext);
  // A W-Soft board keeps no Writers list (that service is not built there).
  const showWriters = !features.wsoft;
  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="information" label="Information" />
        <Tab value="configuration" label="Configuration" />
        <Tab value="rest" label="REST View" />
        <Tab value="stream" label="Live Stream" />
        <Tab value="ble" label="BLE Stream" />
        {showWriters && <Tab value="writers" label="Writers" />}
      </RouterTabs>
      <Routes>
        <Route path="information" element={<SerialInfoWithMode />} />
        <Route path="configuration" element={<SerialConfig />} />
        <Route path="rest" element={<SerialRest />} />
        <Route path="stream" element={<SerialWebSocket />} />
        <Route path="ble" element={<SerialBle />} />
        {showWriters && <Route path="writers" element={<Writers />} />}
        <Route path="/*" element={<Navigate replace to="information" />} />
      </Routes>
    </>
  );
};

export default SerialReader;
