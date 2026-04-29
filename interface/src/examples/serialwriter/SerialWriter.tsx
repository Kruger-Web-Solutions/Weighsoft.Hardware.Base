import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import SerialWriterInfo from './SerialWriterInfo';
import SerialWriterConfig from './SerialWriterConfig';
import SerialWriterSend from './SerialWriterSend';
import SerialWriterBle from './SerialWriterBle';
import Diagnostics from '../diagnostics/Diagnostics';

const SerialWriter: FC = () => {
  useLayoutTitle('Serial Writer');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="info" label="Information" />
        <Tab value="config" label="Configuration" />
        <Tab value="send" label="Send" />
        <Tab value="diagnostics" label="Diagnostics" />
        <Tab value="ble" label="BLE" />
      </RouterTabs>
      <Routes>
        <Route path="info" element={<SerialWriterInfo />} />
        <Route path="config" element={<SerialWriterConfig />} />
        <Route path="send" element={<SerialWriterSend />} />
        <Route path="diagnostics/*" element={<Diagnostics />} />
        <Route path="ble" element={<SerialWriterBle />} />
        <Route path="/*" element={<Navigate replace to="info" />} />
      </Routes>
    </>
  );
};

export default SerialWriter;
