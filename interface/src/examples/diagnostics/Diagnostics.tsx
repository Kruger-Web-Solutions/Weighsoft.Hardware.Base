import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import DiagnosticsInfo from './DiagnosticsInfo';
import LoopbackTest from './LoopbackTest';
import BaudDetector from './BaudDetector';
import SignalQuality from './SignalQuality';

const Diagnostics: FC = () => {
  useLayoutTitle('UART Diagnostics');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="info" label="Info" />
        <Tab value="loopback" label="Loopback Test" />
        <Tab value="baud" label="Baud Detector" />
        <Tab value="signal" label="Signal Quality" />
      </RouterTabs>
      <Routes>
        <Route path="info" element={<DiagnosticsInfo />} />
        <Route path="loopback" element={<LoopbackTest />} />
        <Route path="baud" element={<BaudDetector />} />
        <Route path="signal" element={<SignalQuality />} />
        <Route path="/*" element={<Navigate replace to="info" />} />
      </Routes>
    </>
  );
};

export default Diagnostics;
