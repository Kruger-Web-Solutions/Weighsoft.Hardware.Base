import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import ScaleIcon from '@mui/icons-material/Scale';
import SettingsIcon from '@mui/icons-material/Settings';
import SettingsInputAntennaIcon from '@mui/icons-material/SettingsInputAntenna';

import { Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticatedContext } from '../../contexts/authentication';

import LiveWeightInfo from './LiveWeightInfo';
import LiveWeightScreen from './LiveWeightScreen';
import LiveWeightSetup from './LiveWeightSetup';

const LiveWeight: FC = () => {
  useLayoutTitle('Live Weight');
  const { routerTab } = useRouterTab();
  const { me } = useContext(AuthenticatedContext);
  const isAdmin = !!me?.admin;

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="live" label="Live" icon={<ScaleIcon />} iconPosition="start" />
        {isAdmin && <Tab value="setup" label="Setup" icon={<SettingsIcon />} iconPosition="start" />}
        {isAdmin && (
          <Tab value="info" label="How it connects" icon={<SettingsInputAntennaIcon />} iconPosition="start" />
        )}
      </RouterTabs>
      <Routes>
        <Route path="live" element={<LiveWeightScreen />} />
        {isAdmin && <Route path="setup" element={<LiveWeightSetup />} />}
        {isAdmin && <Route path="info" element={<LiveWeightInfo />} />}
        <Route path="/*" element={<Navigate to="live" replace />} />
      </Routes>
    </>
  );
};

export default LiveWeight;
