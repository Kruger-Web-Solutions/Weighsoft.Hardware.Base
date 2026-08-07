import { FC } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import ElectricalServicesIcon from '@mui/icons-material/ElectricalServices';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={ElectricalServicesIcon} label="Relay Board Twin" to={`/${PROJECT_PATH}/relay-board`} />
    <LayoutMenuItem icon={LightbulbIcon} label="LED Example" to={`/${PROJECT_PATH}/led-example`} />
  </List>
);

export default ProjectMenu;
