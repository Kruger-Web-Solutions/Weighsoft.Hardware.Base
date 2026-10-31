import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import WSoftInfo from './WSoftInfo';
import WSoftLive from './WSoftLive';
import WSoftSettingsForm from './WSoftSettingsForm';
import WSoftHub from './WSoftHub';
import WSoftCalibrate from './WSoftCalibrate';

const WSoft: FC = () => {
  useLayoutTitle('W-Soft');
  const { routerTab } = useRouterTab();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="live" label="Live" />
        {isAdmin && <Tab value="settings" label="Settings" />}
        {isAdmin && <Tab value="hub" label="Hub" />}
        {isAdmin && <Tab value="calibrate" label="Calibrate" />}
        <Tab value="info" label="Information" />
      </RouterTabs>
      <Routes>
        <Route path="live" element={<WSoftLive />} />
        {isAdmin && <Route path="settings" element={<WSoftSettingsForm />} />}
        {isAdmin && <Route path="hub" element={<WSoftHub />} />}
        {isAdmin && <Route path="calibrate" element={<WSoftCalibrate />} />}
        <Route path="info" element={<WSoftInfo />} />
        <Route path="/*" element={<Navigate replace to="live" />} />
      </Routes>
    </>
  );
};

export default WSoft;
