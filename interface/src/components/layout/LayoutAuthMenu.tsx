import React, { FC, useContext } from "react";
import { Link as RouterLink } from "react-router-dom";

import { Box, Button, Divider, IconButton, Popover, Typography, Avatar, styled, TypographyProps } from '@mui/material';

import PersonIcon from "@mui/icons-material/Person";
import AccountCircleIcon from "@mui/icons-material/AccountCircle";
import LoginIcon from "@mui/icons-material/Login";

import { AuthenticationContext } from "../../contexts/authentication";

const ItemTypography = styled(Typography)<TypographyProps>({
  maxWidth: '250px',
  whiteSpace: 'nowrap',
  overflow: 'hidden',
  textOverflow: 'ellipsis',
});

const LayoutAuthMenu: FC = () => {
  const { me, signOut } = useContext(AuthenticationContext);

  const [anchorEl, setAnchorEl] = React.useState<HTMLButtonElement | null>(null);

  const handleClick = (event: React.MouseEvent<HTMLButtonElement>) => {
    setAnchorEl(event.currentTarget);
  };

  const handleClose = () => {
    setAnchorEl(null);
  };

  const open = Boolean(anchorEl);
  const id = anchorEl ? 'app-menu-popover' : undefined;

  if (!me) {
    return (
      <Button
        id="open-login"
        color="inherit"
        component={RouterLink}
        to="/"
        startIcon={<LoginIcon />}
        sx={{ textTransform: 'none' }}
      >
        Log in
      </Button>
    );
  }

  return (
    <>
      <IconButton
        id="open-auth-menu"
        sx={{ padding: 0 }}
        aria-describedby={id}
        color="inherit"
        onClick={handleClick}
      >
        <AccountCircleIcon />
      </IconButton>
      <Popover
        id="app-menu-popover"
        sx={{ mt: 1 }}
        open={open}
        anchorEl={anchorEl}
        onClose={handleClose}
        anchorOrigin={{
          vertical: 'bottom',
          horizontal: 'center',
        }}
        transformOrigin={{
          vertical: 'top',
          horizontal: 'center',
        }}
      >
        <Box display="flex" flexDirection="row" alignItems="center" p={2}>
          <Avatar sx={{ width: 80, height: 80 }}>
            <PersonIcon fontSize="large" />
          </Avatar>
          <Box pl={2}>
            <ItemTypography variant="h6">
              {me.username}
            </ItemTypography>
            <ItemTypography variant="body1">
              {me.admin ? "Admin User" : "Guest User"}
            </ItemTypography>
          </Box>
        </Box>
        <Divider />
        <Box p={1.5}>
          <Button variant="contained" fullWidth color="primary" onClick={() => signOut(true)}>Sign Out</Button>
        </Box>
      </Popover>
    </>
  );
};

export default LayoutAuthMenu;
