import { FC } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import ScaleIcon from '@mui/icons-material/Scale';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={LightbulbIcon} label="LED Example" to={`/${PROJECT_PATH}/led-example`} />
    <LayoutMenuItem icon={ScaleIcon} label="Weighing" to={`/${PROJECT_PATH}/weighing`} />
  </List>
);

export default ProjectMenu;
