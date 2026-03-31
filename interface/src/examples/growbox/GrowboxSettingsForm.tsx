import { ChangeEvent, FC, useState } from 'react';

import {
  Box,
  Button,
  Checkbox,
  Divider,
  Grid,
  TextField,
  Typography
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import BoltIcon from '@mui/icons-material/Bolt';

import {
  BlockFormControlLabel,
  ButtonRow,
  FormLoader,
  MessageBox,
  SectionContent
} from '../../components';
import { updateValue, useRest } from '../../utils';

import * as GrowboxApi from './api';
import { GrowboxSettings, GrowboxStateUpdate } from './types';

const GrowboxSettingsForm: FC = () => {
  const { loadData, saveData, saving, setData, data, errorMessage } =
    useRest<GrowboxSettings>({
      read: GrowboxApi.readGrowboxSettings,
      update: GrowboxApi.updateGrowboxSettings
    });

  const [testRelay, setTestRelay] = useState<string | null>(null);

  const updateFormValue = updateValue(setData);

  const handlePinChange = (field: keyof GrowboxSettings) => (e: ChangeEvent<HTMLInputElement>) => {
    if (!data) return;
    const v = parseInt(e.target.value, 10);
    if (!isNaN(v)) setData({ ...data, [field]: v });
  };

  const sendRelayTest = async (field: keyof GrowboxStateUpdate, label: string) => {
    setTestRelay(label);
    try {
      const onUpdate: GrowboxStateUpdate = { lights_on: false, fan_in_on: false, fan_out_on: false };
      onUpdate[field] = true;
      if (field === 'lights_on') {
        onUpdate.light_a_on = true;
        onUpdate.light_b_on = true;
      }
      await GrowboxApi.updateGrowboxState(onUpdate);
      await new Promise((r) => setTimeout(r, 2000));
      await GrowboxApi.updateGrowboxState({ lights_on: false, light_a_on: false, light_b_on: false, fan_in_on: false, fan_out_on: false });
    } finally {
      setTestRelay(null);
    }
  };

  const sendCalibration = async (field: 'moisture_dry_value' | 'moisture_wet_value') => {
    if (!data) return;
    const stateRes = await GrowboxApi.readGrowboxState();
    const raw = stateRes.data.moisture_raw;
    setData({ ...data, [field]: raw });
  };

  const content = () => {
    if (!data) {
      return <FormLoader onRetry={loadData} errorMessage={errorMessage} />;
    }

    return (
      <>
        <MessageBox
          level="info"
          message="GPIO pin changes take effect immediately and are saved to flash. Relay test briefly fires each relay for 2 seconds."
          my={2}
        />

        {/* ── GPIO Assignments ──────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          GPIO Pin Assignments
        </Typography>
        <Typography variant="body2" color="text.secondary" mb={2}>
          Use ESP32-S3 safe GPIO pins. Avoid 0, 3, 19, 20, 45, 46 and flash-reserved 35–37.
        </Typography>

        <Grid container spacing={2} mb={2}>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Light Relay A"
              type="number"
              fullWidth
              size="small"
              value={data.light_relay_a_pin}
              onChange={handlePinChange('light_relay_a_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Light Relay B"
              type="number"
              fullWidth
              size="small"
              value={data.light_relay_b_pin}
              onChange={handlePinChange('light_relay_b_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Fan In"
              type="number"
              fullWidth
              size="small"
              value={data.fan_in_pin}
              onChange={handlePinChange('fan_in_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Fan Out"
              type="number"
              fullWidth
              size="small"
              value={data.fan_out_pin}
              onChange={handlePinChange('fan_out_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="I2C SDA (SHT20)"
              type="number"
              fullWidth
              size="small"
              value={data.i2c_sda_pin}
              onChange={handlePinChange('i2c_sda_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="I2C SCL (SHT20)"
              type="number"
              fullWidth
              size="small"
              value={data.i2c_scl_pin}
              onChange={handlePinChange('i2c_scl_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="DHT22 Data"
              type="number"
              fullWidth
              size="small"
              value={data.dht_pin}
              onChange={handlePinChange('dht_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Moisture ADC"
              type="number"
              fullWidth
              size="small"
              value={data.moisture_adc_pin}
              onChange={handlePinChange('moisture_adc_pin')}
              disabled={saving}
              inputProps={{ min: 0, max: 48 }}
            />
          </Grid>
        </Grid>

        <Divider sx={{ my: 2 }} />

        {/* ── Relay Options ─────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Relay Options
        </Typography>

        <BlockFormControlLabel
          control={
            <Checkbox
              name="relay_active_high"
              checked={data.relay_active_high}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Relay active-high (uncheck for active-low boards)"
        />

        <TextField
          label="Sensor poll interval (ms)"
          type="number"
          size="small"
          value={data.poll_interval_ms}
          onChange={(e) => {
            const v = parseInt(e.target.value, 10);
            if (!isNaN(v) && data) setData({ ...data, poll_interval_ms: v });
          }}
          disabled={saving}
          inputProps={{ min: 1000, max: 60000, step: 1000 }}
          sx={{ mt: 1 }}
        />

        <Divider sx={{ my: 2 }} />

        {/* ── Sensor Enables ────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Sensor Enables
        </Typography>

        <BlockFormControlLabel
          control={
            <Checkbox
              name="inside_sensor_enabled"
              checked={data.inside_sensor_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Inside sensor (SHT20, I2C)"
        />
        <BlockFormControlLabel
          control={
            <Checkbox
              name="outside_sensor_enabled"
              checked={data.outside_sensor_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Outside sensor (DHT22, one-wire)"
        />
        <BlockFormControlLabel
          control={
            <Checkbox
              name="moisture_sensor_enabled"
              checked={data.moisture_sensor_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Moisture sensor (capacitive, ADC)"
        />

        <Divider sx={{ my: 2 }} />

        {/* ── Moisture Calibration ──────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Moisture Sensor Calibration
        </Typography>
        <Typography variant="body2" color="text.secondary" mb={2}>
          Step 1: Hold the probe in open air → click <strong>Set as Dry</strong>.<br />
          Step 2: Submerge the probe sensing section in water → click <strong>Set as Wet</strong>.<br />
          Raw ADC values are shown live on the Dashboard.
        </Typography>

        <Grid container spacing={2} alignItems="center" mb={2}>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Dry ADC value (air)"
              type="number"
              fullWidth
              size="small"
              value={data.moisture_dry_value}
              onChange={(e) => {
                const v = parseInt(e.target.value, 10);
                if (!isNaN(v) && data) setData({ ...data, moisture_dry_value: v });
              }}
              disabled={saving}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <Button
              variant="outlined"
              size="small"
              onClick={() => sendCalibration('moisture_dry_value')}
              disabled={saving}
            >
              Set as Dry
            </Button>
          </Grid>
          <Grid item xs={6} sm={3}>
            <TextField
              label="Wet ADC value (water)"
              type="number"
              fullWidth
              size="small"
              value={data.moisture_wet_value}
              onChange={(e) => {
                const v = parseInt(e.target.value, 10);
                if (!isNaN(v) && data) setData({ ...data, moisture_wet_value: v });
              }}
              disabled={saving}
            />
          </Grid>
          <Grid item xs={6} sm={3}>
            <Button
              variant="outlined"
              size="small"
              onClick={() => sendCalibration('moisture_wet_value')}
              disabled={saving}
            >
              Set as Wet
            </Button>
          </Grid>
        </Grid>

        <Divider sx={{ my: 2 }} />

        {/* ── Relay Test ────────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Relay Test
        </Typography>
        <MessageBox
          level="warning"
          message="Test buttons fire each relay for 2 seconds and then turn it off. Automation is not affected."
          my={1}
        />

        <Box display="flex" gap={2} flexWrap="wrap" mt={1} mb={2}>
          <Button
            variant="outlined"
            startIcon={<BoltIcon />}
            disabled={testRelay !== null}
            onClick={() => sendRelayTest('lights_on', 'Lights')}
            color="warning"
          >
            {testRelay === 'Lights' ? 'Testing Lights…' : 'Test Lights'}
          </Button>
          <Button
            variant="outlined"
            startIcon={<BoltIcon />}
            disabled={testRelay !== null}
            onClick={() => sendRelayTest('fan_in_on', 'Fan In')}
          >
            {testRelay === 'Fan In' ? 'Testing Fan In…' : 'Test Fan In'}
          </Button>
          <Button
            variant="outlined"
            startIcon={<BoltIcon />}
            disabled={testRelay !== null}
            onClick={() => sendRelayTest('fan_out_on', 'Fan Out')}
          >
            {testRelay === 'Fan Out' ? 'Testing Fan Out…' : 'Test Fan Out'}
          </Button>
        </Box>

        <Divider sx={{ my: 2 }} />

        {/* ── Save ──────────────────────────────────────────── */}
        <ButtonRow mt={1}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
            variant="contained"
            color="primary"
            onClick={saveData}
          >
            Save Settings
          </Button>
        </ButtonRow>
      </>
    );
  };

  return (
    <SectionContent title="Hardware Settings" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default GrowboxSettingsForm;
