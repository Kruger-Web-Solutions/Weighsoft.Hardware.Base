import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';
import WeightForwarderConfig from './WeightForwarderConfig';
import WeightForwarderStatus from './WeightForwarderStatus';

const WeightForwarder: FC = () => {
  useLayoutTitle('Weight Forwarder');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="config" label="Configuration" />
        <Tab value="status" label="Status" />
      </RouterTabs>
      <Routes>
        <Route path="config" element={<WeightForwarderConfig />} />
        <Route path="status" element={<WeightForwarderStatus />} />
        <Route path="/*" element={<Navigate replace to="config" />} />
      </Routes>
    </>
  );
};

export default WeightForwarder;
