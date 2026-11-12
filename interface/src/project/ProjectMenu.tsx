import { FC, useContext } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import SerialPortIcon from '@mui/icons-material/Cable';
import BuildIcon from '@mui/icons-material/Build';
import SendIcon from '@mui/icons-material/Send';
import MonitorWeightIcon from '@mui/icons-material/Monitor';
import TvIcon from '@mui/icons-material/Tv';
import ScaleIcon from '@mui/icons-material/Scale';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';
import { FeaturesContext } from '../contexts/features';

// A W-Soft board does not create the Serial1 devices (Diagnostics, Weight Forwarder), the
// Remote Weight receiver or the LCD device, so their pages only answer 404 there: the menu
// leaves them out. Serial stays on boards wired for an RS232 scale (its page sets the speed
// and the weight pattern). Every other board keeps the full list.
const ProjectMenu: FC = () => {
  const { features } = useContext(FeaturesContext);
  const wsoft = !!features.wsoft;
  return (
    <List>
      {!wsoft && <LayoutMenuItem icon={LightbulbIcon} label="LED" to={`/${PROJECT_PATH}/led-example`} />}
      {(!wsoft || features.wsoft_rs232) && (
        <LayoutMenuItem icon={SerialPortIcon} label="Serial" to={`/${PROJECT_PATH}/serial`} />
      )}
      {!wsoft && <LayoutMenuItem icon={BuildIcon} label="Diagnostics" to={`/${PROJECT_PATH}/diagnostics`} />}
      {!wsoft && (
        <LayoutMenuItem icon={SendIcon} label="Weight Forwarder" to={`/${PROJECT_PATH}/weightforwarder`} />
      )}
      {!wsoft && (
        <LayoutMenuItem icon={MonitorWeightIcon} label="Remote Weight" to={`/${PROJECT_PATH}/remote-weight`} />
      )}
      {!wsoft && <LayoutMenuItem icon={TvIcon} label="LCD Display" to={`/${PROJECT_PATH}/lcd-display`} />}
      <LayoutMenuItem icon={ScaleIcon} label="W-Soft" to={`/${PROJECT_PATH}/wsoft`} />
    </List>
  );
};

export default ProjectMenu;
