import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';
import SerialWriterForwarderConfig from './SerialWriterForwarderConfig';
import SerialWriterForwarderStatus from './SerialWriterForwarderStatus';

const SerialWriterForwarder: FC = () => {
  useLayoutTitle('Serial Writer Forwarder');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="config" label="Configuration" />
        <Tab value="status" label="Status" />
      </RouterTabs>
      <Routes>
        <Route path="config" element={<SerialWriterForwarderConfig />} />
        <Route path="status" element={<SerialWriterForwarderStatus />} />
        <Route path="/*" element={<Navigate replace to="config" />} />
      </Routes>
    </>
  );
};

export default SerialWriterForwarder;
