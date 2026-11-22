import { FC, useContext } from 'react';
import { Navigate } from 'react-router-dom';

import { useLayoutTitle } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';
import { PROJECT_PATH } from '../../api/env';

import WSoftHubForm from './WSoftHubForm';

// The Hub link (pairing code, status) on its own sidebar item, admin only (Rhyno, 12 Nov 2026).
// Same form as the old W-Soft > Hub tab; that address now leads here.
const WSoftHubPage: FC = () => {
  useLayoutTitle('Hub');
  const { me } = useContext(AuthenticationContext);
  if (!me?.admin) return <Navigate replace to={`/${PROJECT_PATH}/wsoft`} />;
  return <WSoftHubForm />;
};

export default WSoftHubPage;
