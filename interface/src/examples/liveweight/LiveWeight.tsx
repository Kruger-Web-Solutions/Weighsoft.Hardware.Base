import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import CategoryIcon from '@mui/icons-material/Category';
import ScaleIcon from '@mui/icons-material/Scale';
import SettingsIcon from '@mui/icons-material/Settings';
import SettingsInputAntennaIcon from '@mui/icons-material/SettingsInputAntenna';
import TuneIcon from '@mui/icons-material/Tune';

import { Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticatedContext } from '../../contexts/authentication';

import LiveWeightInfo from './LiveWeightInfo';
import LiveWeightProduct from './LiveWeightProduct';
import LiveWeightScreen from './LiveWeightScreen';
import LiveWeightSetup from './LiveWeightSetup';
import LiveWeightTarget from './LiveWeightTarget';

const LiveWeight: FC = () => {
  useLayoutTitle('Live Weight');
  const { routerTab } = useRouterTab();
  const { me } = useContext(AuthenticatedContext);
  const isAdmin = !!me?.admin;

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="live" label="Live" icon={<ScaleIcon />} iconPosition="start" />
        <Tab value="target" label="Target & Relays" icon={<TuneIcon />} iconPosition="start" />
        <Tab value="product" label="Product" icon={<CategoryIcon />} iconPosition="start" />
        {isAdmin && <Tab value="tech" label="Tech" icon={<SettingsIcon />} iconPosition="start" />}
        {isAdmin && (
          <Tab value="info" label="How it connects" icon={<SettingsInputAntennaIcon />} iconPosition="start" />
        )}
      </RouterTabs>
      <Routes>
        <Route path="live" element={<LiveWeightScreen />} />
        <Route path="target" element={<LiveWeightTarget />} />
        <Route path="product" element={<LiveWeightProduct />} />
        {isAdmin && <Route path="tech" element={<LiveWeightSetup />} />}
        {isAdmin && <Route path="info" element={<LiveWeightInfo />} />}
        <Route path="/*" element={<Navigate to="live" replace />} />
      </Routes>
    </>
  );
};

export default LiveWeight;
