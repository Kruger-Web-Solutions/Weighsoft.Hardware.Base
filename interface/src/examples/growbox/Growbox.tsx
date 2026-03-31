import { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import GrowboxDashboard from './GrowboxDashboard';
import GrowboxAutomationForm from './GrowboxAutomationForm';
import GrowboxBleInfo from './GrowboxBleInfo';
import GrowboxSettingsForm from './GrowboxSettingsForm';

const Growbox: FC = () => {
  useLayoutTitle('Growbox');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="dashboard" label="Dashboard" />
        <Tab value="automation" label="Automation" />
        <Tab value="ble" label="BLE" />
        <Tab value="settings" label="Settings" />
      </RouterTabs>
      <Routes>
        <Route path="dashboard" element={<GrowboxDashboard />} />
        <Route path="automation" element={<GrowboxAutomationForm />} />
        <Route path="ble" element={<GrowboxBleInfo />} />
        <Route path="settings" element={<GrowboxSettingsForm />} />
        <Route path="/*" element={<Navigate replace to="dashboard" />} />
      </Routes>
    </>
  );
};

export default Growbox;
