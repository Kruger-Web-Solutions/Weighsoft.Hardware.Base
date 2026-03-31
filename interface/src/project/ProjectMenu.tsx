import { FC } from 'react';

import { List } from '@mui/material';
import LightbulbIcon from '@mui/icons-material/Lightbulb';
import SerialPortIcon from '@mui/icons-material/Cable';
import BuildIcon from '@mui/icons-material/Build';
import SendIcon from '@mui/icons-material/Send';
import OutputIcon from '@mui/icons-material/Output';
import CloudDownloadIcon from '@mui/icons-material/CloudDownload';

import { PROJECT_PATH } from '../api/env';
import LayoutMenuItem from '../components/layout/LayoutMenuItem';

const ProjectMenu: FC = () => (
  <List>
    <LayoutMenuItem icon={LightbulbIcon} label="LED" to={`/${PROJECT_PATH}/led-example`} />
    <LayoutMenuItem icon={SerialPortIcon} label="Serial" to={`/${PROJECT_PATH}/serial`} />
    <LayoutMenuItem icon={BuildIcon} label="Diagnostics" to={`/${PROJECT_PATH}/diagnostics`} />
    <LayoutMenuItem icon={SendIcon} label="Weight Forwarder" to={`/${PROJECT_PATH}/weightforwarder`} />
    <LayoutMenuItem icon={OutputIcon} label="Serial Writer" to={`/${PROJECT_PATH}/serialwriter`} />
    <LayoutMenuItem icon={CloudDownloadIcon} label="Writer Forwarder" to={`/${PROJECT_PATH}/serialwriterforwarder`} />
  </List>
);

export default ProjectMenu;
