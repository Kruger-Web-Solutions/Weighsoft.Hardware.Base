import { FC, useContext } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Link as RouterLink } from 'react-router-dom';

import CategoryIcon from '@mui/icons-material/Category';
import LoginIcon from '@mui/icons-material/Login';
import ScaleIcon from '@mui/icons-material/Scale';
import SettingsIcon from '@mui/icons-material/Settings';
import SettingsInputAntennaIcon from '@mui/icons-material/SettingsInputAntenna';
import TuneIcon from '@mui/icons-material/Tune';

import { Alert, Button, Tab } from '@mui/material';

import { RouterTabs, useLayoutTitle, useRouterTab } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import LiveWeightInfo from './LiveWeightInfo';
import LiveWeightProduct from './LiveWeightProduct';
import LiveWeightScreen from './LiveWeightScreen';
import LiveWeightSetup from './LiveWeightSetup';
import LiveWeightTarget from './LiveWeightTarget';

const LiveWeight: FC = () => {
  useLayoutTitle('Live Weight');
  const { routerTab } = useRouterTab();
  const { me } = useContext(AuthenticationContext);
  const isLoggedIn = !!me;
  const isAdmin = !!me?.admin;

  return (
    <>
      {!isLoggedIn && (
        <Alert
          severity="info"
          sx={{ mb: 2 }}
          action={
            <Button color="inherit" size="small" component={RouterLink} to="/" startIcon={<LoginIcon />}>
              Log in
            </Button>
          }
        >
          Operator mode — weigh, PLU, and transactions. Log in for settings and config.
        </Alert>
      )}
      <RouterTabs value={routerTab}>
        <Tab value="live" label="Live" icon={<ScaleIcon />} iconPosition="start" />
        {isLoggedIn && (
          <Tab value="target" label="Target & Relays" icon={<TuneIcon />} iconPosition="start" />
        )}
        <Tab value="product" label="Product" icon={<CategoryIcon />} iconPosition="start" />
        {isAdmin && <Tab value="tech" label="Tech" icon={<SettingsIcon />} iconPosition="start" />}
        {isAdmin && (
          <Tab value="info" label="How it connects" icon={<SettingsInputAntennaIcon />} iconPosition="start" />
        )}
      </RouterTabs>
      <Routes>
        <Route path="live" element={<LiveWeightScreen />} />
        {isLoggedIn && <Route path="target" element={<LiveWeightTarget />} />}
        <Route path="product" element={<LiveWeightProduct />} />
        {isAdmin && <Route path="tech" element={<LiveWeightSetup />} />}
        {isAdmin && <Route path="info" element={<LiveWeightInfo />} />}
        <Route path="/*" element={<Navigate to="live" replace />} />
      </Routes>
    </>
  );
};

export default LiveWeight;
