import { FC } from 'react';

import { List } from '@mui/material';
import ElectricalServicesIcon from '@mui/icons-material/ElectricalServices';
import ScaleIcon from '@mui/icons-material/Scale';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={ElectricalServicesIcon} label="Relay Board Twin" to={`/${PROJECT_PATH}/relay-board`} />
    <LayoutMenuItem icon={ScaleIcon} label="Live Weight" to={`/${PROJECT_PATH}/live-weight`} />
  </List>
);

export default ProjectMenu;
