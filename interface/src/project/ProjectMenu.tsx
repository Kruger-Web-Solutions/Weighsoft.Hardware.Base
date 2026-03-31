import { FC } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import SpaIcon from '@mui/icons-material/Spa';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={SpaIcon} label="Growbox" to={`/${PROJECT_PATH}/growbox`} />
    <LayoutMenuItem icon={LightbulbIcon} label="LED Example" to={`/${PROJECT_PATH}/led-example`} />
  </List>
);

export default ProjectMenu;
