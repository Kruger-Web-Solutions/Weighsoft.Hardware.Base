import { FC } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import BuildIcon from '@mui/icons-material/Build';
import OutputIcon from '@mui/icons-material/Output';
import CloudDownloadIcon from '@mui/icons-material/CloudDownload';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={LightbulbIcon} label="LED" to={`/${PROJECT_PATH}/led-example`} />
    <LayoutMenuItem icon={OutputIcon} label="Serial Writer" to={`/${PROJECT_PATH}/serialwriter`} />
    <LayoutMenuItem icon={CloudDownloadIcon} label="Serial Reader Source" to={`/${PROJECT_PATH}/serialwriterforwarder`} />
    <LayoutMenuItem icon={BuildIcon} label="Diagnostics" to={`/${PROJECT_PATH}/diagnostics`} />
  </List>
);

export default ProjectMenu;
