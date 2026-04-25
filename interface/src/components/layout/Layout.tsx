
import { FC, useState, useEffect } from 'react';
import { useLocation } from 'react-router-dom';

import { Box, Toolbar } from '@mui/material';

import { PROJECT_NAME } from '../../api/env';
import { RequiredChildrenProps } from '../../utils';

import LayoutDrawer from './LayoutDrawer';
import LayoutAppBar from './LayoutAppBar';
import VersionDisplay from '../VersionDisplay';
import { LayoutContext } from './context';

export const DRAWER_WIDTH = 280;

const Layout: FC<RequiredChildrenProps> = ({ children }) => {

  const [mobileOpen, setMobileOpen] = useState(false);
  const [title, setTitle] = useState(PROJECT_NAME);
  const { pathname } = useLocation();

  const handleDrawerToggle = () => {
    setMobileOpen(!mobileOpen);
  };

  useEffect(() => setMobileOpen(false), [pathname]);

  return (
    <LayoutContext.Provider value={{ title, setTitle }}>
      <LayoutAppBar title={title} onToggleDrawer={handleDrawerToggle} />
      <LayoutDrawer mobileOpen={mobileOpen} onClose={handleDrawerToggle} />
      <Box
        component="main"
        sx={{ 
          marginLeft: { md: `${DRAWER_WIDTH}px` },
          display: 'flex',
          flexDirection: 'column',
          minHeight: '100vh'
        }}
      >
        <Toolbar />
        <Box sx={{ flexGrow: 1 }}>
          {children}
        </Box>
        <VersionDisplay />
      </Box>
    </LayoutContext.Provider >
  );

};

export default Layout;
