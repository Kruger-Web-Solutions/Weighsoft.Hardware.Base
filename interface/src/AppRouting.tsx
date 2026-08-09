import { FC, useContext, useEffect } from 'react';
import { Navigate, Routes, Route, useLocation } from 'react-router-dom';
import { useSnackbar, VariantType } from 'notistack';

import { Authentication, AuthenticationContext } from './contexts/authentication';
import { FeaturesContext } from './contexts/features';
import { Layout, RequireAuthenticated, RequireUnauthenticated } from './components';
import { PROJECT_PATH } from './api/env';

import SignIn from './SignIn';
import AuthenticatedRouting from './AuthenticatedRouting';
import LiveWeight from './examples/liveweight/LiveWeight';

interface SecurityRedirectProps {
  message: string;
  variant?: VariantType;
  signOut?: boolean;
}

const RootRedirect: FC<SecurityRedirectProps> = ({ message, variant, signOut }) => {
  const authenticationContext = useContext(AuthenticationContext);
  const { enqueueSnackbar } = useSnackbar();
  useEffect(() => {
    signOut && authenticationContext.signOut(false);
    enqueueSnackbar(message, { variant });
  }, [message, variant, signOut, authenticationContext, enqueueSnackbar]);
  return <Navigate to="/" />;
};

export const RemoveTrailingSlashes = () => {
  const location = useLocation();
  return (
    location.pathname.match('/.*/$') && (
      <Navigate
        to={{
          pathname: location.pathname.replace(/\/+$/, ''),
          search: location.search
        }}
      />
    )
  );
};

/** End-user Live Weight — no login. Settings stay behind RequireAuthenticated. */
const PublicLiveWeight: FC = () => (
  <Layout>
    <LiveWeight />
  </Layout>
);

const AppRouting: FC = () => {
  const { features } = useContext(FeaturesContext);

  return (
    <Authentication>
      <RemoveTrailingSlashes />
      <Routes>
        <Route path="/unauthorized" element={<RootRedirect message="Please log in to continue" signOut />} />
        <Route
          path="/firmwareUpdated"
          element={<RootRedirect message="Firmware update successful" variant="success" />}
        />
        {features.security && (
          <Route
            path="/"
            element={
              <RequireUnauthenticated>
                <SignIn />
              </RequireUnauthenticated>
            }
          />
        )}
        {features.project && (
          <Route path={`/${PROJECT_PATH}/live-weight/*`} element={<PublicLiveWeight />} />
        )}
        <Route
          path="/*"
          element={
            <RequireAuthenticated>
              <AuthenticatedRouting />
            </RequireAuthenticated>
          }
        />
      </Routes>
    </Authentication>
  );
};

export default AppRouting;
