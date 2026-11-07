import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import WSoftHubForm from './WSoftHubForm';
import WSoftInfo from './WSoftInfo';
import WSoftLive from './WSoftLive';
import WSoftScaleSetup from './WSoftScaleSetup';
import WSoftSettingsForm from './WSoftSettingsForm';

const WSoft: FC = () => {
  useLayoutTitle('W-Soft');
  const { routerTab } = useRouterTab();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="live" label="Live" />
        <Tab value="setup" label="Scale setup" />
        {isAdmin && <Tab value="settings" label="Settings" />}
        {isAdmin && <Tab value="hub" label="Hub" />}
        <Tab value="info" label="Information" />
      </RouterTabs>
      <Routes>
        <Route path="live" element={<WSoftLive />} />
        <Route path="setup" element={<WSoftScaleSetup />} />
        {isAdmin && <Route path="settings" element={<WSoftSettingsForm />} />}
        {isAdmin && <Route path="hub" element={<WSoftHubForm />} />}
        <Route path="info" element={<WSoftInfo />} />
        <Route path="/*" element={<Navigate replace to="live" />} />
      </Routes>
    </>
  );
};

export default WSoft;
