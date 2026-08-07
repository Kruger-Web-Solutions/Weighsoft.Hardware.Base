import { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';

import AccountTreeIcon from '@mui/icons-material/AccountTree';
import ViewInArIcon from '@mui/icons-material/ViewInAr';

import { Tab } from '@mui/material';

import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import RelayBoardDigitalTwin from './RelayBoardDigitalTwin';
import RelayBoardWiring from './RelayBoardWiring';

const RelayBoard: FC = () => {
  useLayoutTitle('Relay Board');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="twin" label="Digital Twin" icon={<ViewInArIcon />} iconPosition="start" />
        <Tab value="wiring" label="Wiring" icon={<AccountTreeIcon />} iconPosition="start" />
      </RouterTabs>
      <Routes>
        <Route path="twin" element={<RelayBoardDigitalTwin />} />
        <Route path="wiring" element={<RelayBoardWiring />} />
        <Route path="/*" element={<Navigate to="twin" replace />} />
      </Routes>
    </>
  );
};

export default RelayBoard;
