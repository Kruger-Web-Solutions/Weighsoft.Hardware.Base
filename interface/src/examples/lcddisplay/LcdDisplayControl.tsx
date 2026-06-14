import React, { FC, useState } from 'react';
import {
  Button,
  Checkbox,
  MenuItem,
  FormControlLabel,
  TextField,
  Box,
  Typography,
  Paper,
  Tab,
  Tabs,
  Alert,
} from '@mui/material';

import { SectionContent, FormLoader } from '../../components';
import { readLcdDisplayData, updateLcdDisplayData } from '../../api/lcddisplay';
import { LcdDisplayData } from '../../types/lcddisplay';
import { LCD_DISPLAY_SOCKET_PATH } from '../../api/endpoints';
import { useRest, useWs } from '../../utils';

interface LcdDisplayControlState {
  mode: 'rest' | 'websocket';
}

const LcdDisplayControl: FC = () => {
  const [controlState, setControlState] = useState<LcdDisplayControlState>({ mode: 'rest' });

  const {
    loadData: loadRestData,
    saveData,
    data: restData,
    setData: setRestData,
    saving,
    errorMessage,
  } = useRest<LcdDisplayData>({ read: readLcdDisplayData, update: updateLcdDisplayData });

  const { data: wsData, updateData: updateWsData } = useWs<LcdDisplayData>(LCD_DISPLAY_SOCKET_PATH);

  const data = controlState.mode === 'rest' ? restData : wsData;
  const setData = controlState.mode === 'rest' ? setRestData : updateWsData;

  if (!data) {
    return (
      <SectionContent title="Display Control" titleGutter>
        <FormLoader />
      </SectionContent>
    );
  }

  const updateField = (field: keyof LcdDisplayData, value: string | number | boolean) => {
    if (setData) {
      setData({ ...data, [field]: value });
    }
  };

  const handleModeChange = (event: React.SyntheticEvent, newMode: 'rest' | 'websocket') => {
    setControlState({ mode: newMode });
    if (newMode === 'rest' && !restData) {
      loadRestData();
    }
  };

  const handleSave = async () => {
    if (controlState.mode === 'rest') {
      await saveData();
    }
  };

  const line1Length = data.line1?.length || 0;
  const line2Length = data.line2?.length || 0;

  return (
    <SectionContent title="Display Control" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Control the LCD display using REST (manual save) or WebSocket (real-time updates).
      </Alert>

      <Tabs value={controlState.mode} onChange={handleModeChange} sx={{ mb: 2 }}>
        <Tab label="REST Mode" value="rest" />
        <Tab label="WebSocket Mode" value="websocket" />
      </Tabs>

      <TextField
        label="Line 1"
        fullWidth
        variant="outlined"
        value={data.line1 || ''}
        onChange={(e) => updateField('line1', e.target.value)}
        helperText={`${line1Length}/16 characters${line1Length > 16 ? ' (will be truncated)' : ''}`}
        sx={{ mb: 2 }}
      />

      <TextField
        label="Line 2"
        fullWidth
        variant="outlined"
        value={data.line2 || ''}
        onChange={(e) => updateField('line2', e.target.value)}
        helperText={`${line2Length}/16 characters${line2Length > 16 ? ' (will be truncated)' : ''}`}
        sx={{ mb: 2 }}
      />

      <TextField
        label="I2C Address"
        fullWidth
        select
        variant="outlined"
        value={data.i2c_address || 0x27}
        onChange={(e) => updateField('i2c_address', parseInt(e.target.value))}
        helperText="Change if your display uses a different I2C address"
        sx={{ mb: 2 }}
      >
        <MenuItem value={0x27}>0x27 (default)</MenuItem>
        <MenuItem value={0x3F}>0x3F (alternative)</MenuItem>
      </TextField>

      <FormControlLabel
        control={
          <Checkbox
            checked={data.backlight}
            onChange={(e) => updateField('backlight', e.target.checked)}
          />
        }
        label="Backlight"
        sx={{ mb: 2 }}
      />

      <Paper elevation={2} sx={{ p: 2, mb: 2, bgcolor: 'background.default' }}>
        <Typography variant="subtitle2" gutterBottom color="text.primary">
          Display Preview:
        </Typography>
        <Box
          sx={{
            fontFamily: 'monospace',
            fontSize: '14px',
            bgcolor: data.backlight ? '#4FC3F7' : '#424242',
            color: data.backlight ? '#000' : '#9E9E9E',
            p: 1,
            borderRadius: 1,
            border: '2px solid',
            borderColor: 'divider',
          }}
        >
          <div>{(data.line1 || '').substring(0, 16).padEnd(16, ' ')}</div>
          <div>{(data.line2 || '').substring(0, 16).padEnd(16, ' ')}</div>
        </Box>
      </Paper>

      {controlState.mode === 'rest' && (
        <Button
          variant="contained"
          color="primary"
          onClick={handleSave}
          disabled={saving}
        >
          {saving ? 'Saving...' : 'Save'}
        </Button>
      )}

      {controlState.mode === 'websocket' && (
        <Alert severity="success" sx={{ mt: 2 }}>
          Changes are sent in real-time via WebSocket.
        </Alert>
      )}

      {errorMessage && (
        <Box sx={{ color: 'error.main', mt: 2 }}>{errorMessage}</Box>
      )}
    </SectionContent>
  );
};

export default LcdDisplayControl;
