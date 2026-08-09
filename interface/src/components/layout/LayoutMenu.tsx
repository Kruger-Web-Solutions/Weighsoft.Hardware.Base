import { FC, useContext } from 'react';

import { Divider, List } from '@mui/material';

import SettingsInputAntennaIcon from '@mui/icons-material/SettingsInputAntenna';
import AccessTimeIcon from '@mui/icons-material/AccessTime';
import DeviceHubIcon from '@mui/icons-material/DeviceHub';
import BluetoothIcon from '@mui/icons-material/Bluetooth';
import SettingsIcon from '@mui/icons-material/Settings';
import LockIcon from '@mui/icons-material/Lock';
import WifiIcon from '@mui/icons-material/Wifi';

import { FeaturesContext } from '../../contexts/features';
import ProjectMenu from '../../project/ProjectMenu';
import { AuthenticationContext } from '../../contexts/authentication';
import LayoutMenuItem from './LayoutMenuItem';

const LayoutMenu: FC = () => {
  const { features } = useContext(FeaturesContext);
  const { me } = useContext(AuthenticationContext);

  return (
    <>
      {features.project && (
        <List disablePadding component="nav">
          <ProjectMenu />
          <Divider />
        </List>
      )}
      <List disablePadding component="nav">
        <LayoutMenuItem icon={WifiIcon} label="WiFi Connection" to="/wifi" />
        <LayoutMenuItem icon={SettingsInputAntennaIcon} label="Access Point" to="/ap" />
        {features.ntp && (
          <LayoutMenuItem icon={AccessTimeIcon} label="Network Time" to="/ntp" />
        )}
        {features.mqtt && (
          <LayoutMenuItem icon={DeviceHubIcon} label="MQTT" to="/mqtt" />
        )}
        {features.ble && (
          <LayoutMenuItem icon={BluetoothIcon} label="BLE" to="/ble" />
        )}
        {features.security && (
          <LayoutMenuItem icon={LockIcon} label="Security" to="/security" disabled={!me?.admin} />
        )}
        <LayoutMenuItem icon={SettingsIcon} label="System" to="/system" />
      </List>
    </>
  );
};

export default LayoutMenu;
