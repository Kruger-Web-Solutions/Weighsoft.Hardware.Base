
import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import LcdDisplayInfo from './LcdDisplayInfo';
import LcdDisplayControl from './LcdDisplayControl';
import { LcdDisplaySerialBridge } from './LcdDisplaySerialBridge';
import LcdDisplayBle from './LcdDisplayBle';

const LcdDisplay: FC = () => {
  useLayoutTitle("LCD Display");
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="information" label="Information" />
        <Tab value="control" label="Control" />
        <Tab value="bridge" label="Serial Bridge" />
        <Tab value="ble" label="BLE Control" />
      </RouterTabs>
      <Routes>
        <Route path="information" element={<LcdDisplayInfo />} />
        <Route path="control" element={<LcdDisplayControl />} />
        <Route path="bridge" element={<LcdDisplaySerialBridge />} />
        <Route path="ble" element={<LcdDisplayBle />} />
        <Route path="/*" element={<Navigate replace to="information" />} />
      </Routes>
    </>
  );
};

export default LcdDisplay;
