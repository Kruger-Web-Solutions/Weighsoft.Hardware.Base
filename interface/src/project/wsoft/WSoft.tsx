import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import { Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';
import { PROJECT_PATH } from '../../api/env';

import WSoftCalibrate from './WSoftCalibrate';
import WSoftInfo from './WSoftInfo';
import WSoftLive from './WSoftLive';
import WSoftProducts from './WSoftProducts';
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
        <Tab value="products" label="Products" />
        <Tab value="setup" label="Scale setup" />
        {isAdmin && <Tab value="calibrate" label="Calibrate" />}
        {isAdmin && <Tab value="settings" label="Settings" />}
        <Tab value="info" label="Information" />
      </RouterTabs>
      <Routes>
        <Route path="live" element={<WSoftLive />} />
        <Route path="products" element={<WSoftProducts />} />
        <Route path="setup" element={<WSoftScaleSetup />} />
        {isAdmin && <Route path="calibrate" element={<WSoftCalibrate />} />}
        {isAdmin && <Route path="settings" element={<WSoftSettingsForm />} />}
        {/* The Hub moved to its own sidebar item; old links still work. */}
        <Route path="hub" element={<Navigate replace to={`/${PROJECT_PATH}/hub`} />} />
        <Route path="info" element={<WSoftInfo />} />
        <Route path="/*" element={<Navigate replace to="live" />} />
      </Routes>
    </>
  );
};

export default WSoft;
