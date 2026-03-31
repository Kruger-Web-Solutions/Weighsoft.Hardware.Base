import { FC } from 'react';

import {
  Box,
  Button,
  Checkbox,
  Divider,
  Slider,
  TextField,
  Typography
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import {
  BlockFormControlLabel,
  ButtonRow,
  FormLoader,
  MessageBox,
  SectionContent
} from '../../components';
import { updateValue, useRest } from '../../utils';

import * as GrowboxApi from './api';
import { GrowboxAutomationSettings } from './types';

const GrowboxAutomationForm: FC = () => {
  const { loadData, saveData, saving, setData, data, errorMessage } =
    useRest<GrowboxAutomationSettings>({
      read: GrowboxApi.readGrowboxAutomation,
      update: GrowboxApi.updateGrowboxAutomation
    });

  const updateFormValue = updateValue(setData);

  const content = () => {
    if (!data) {
      return <FormLoader onRetry={loadData} errorMessage={errorMessage} />;
    }

    return (
      <>
        <MessageBox
          level="info"
          message={
            'Automation runs entirely on the ESP32 — continues working offline. ' +
            'Light scheduling requires NTP (configure timezone in NTP Settings).'
          }
          my={2}
        />

        {/* ── Light Schedule ────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Light Schedule
        </Typography>

        <BlockFormControlLabel
          control={
            <Checkbox
              name="light_schedule_enabled"
              checked={data.light_schedule_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Enable daily light schedule"
        />

        <Box display="flex" gap={2} mt={1} mb={2} flexWrap="wrap">
          <TextField
            label="Lights ON (local time)"
            type="time"
            size="small"
            value={data.light_on_time}
            onChange={(e) => setData({ ...data, light_on_time: e.target.value })}
            disabled={saving || !data.light_schedule_enabled}
            InputLabelProps={{ shrink: true }}
            inputProps={{ step: 60 }}
          />
          <TextField
            label="Lights OFF (local time)"
            type="time"
            size="small"
            value={data.light_off_time}
            onChange={(e) => setData({ ...data, light_off_time: e.target.value })}
            disabled={saving || !data.light_schedule_enabled}
            InputLabelProps={{ shrink: true }}
            inputProps={{ step: 60 }}
          />
        </Box>

        <Divider sx={{ my: 2 }} />

        {/* ── Fan Automation ────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Fan Automation
        </Typography>
        <Typography variant="body2" color="text.secondary" mb={1}>
          Fans turn ON when inside temperature rises above the threshold and turn OFF when temperature
          drops below <em>(threshold − hysteresis)</em>, preventing rapid switching.
        </Typography>

        <BlockFormControlLabel
          control={
            <Checkbox
              name="fan_control_enabled"
              checked={data.fan_control_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Enable temperature-driven fan control"
        />

        <Box mt={1} mb={1}>
          <Typography gutterBottom>
            Fan ON temperature threshold: <strong>{data.fan_temperature_threshold.toFixed(1)} °C</strong>
          </Typography>
          <Slider
            value={data.fan_temperature_threshold}
            onChange={(_, v) => setData({ ...data, fan_temperature_threshold: v as number })}
            min={15}
            max={40}
            step={0.5}
            disabled={saving || !data.fan_control_enabled}
            valueLabelDisplay="auto"
            sx={{ maxWidth: 400 }}
          />
        </Box>

        <Box mb={2}>
          <Typography gutterBottom>
            Hysteresis: <strong>{data.fan_hysteresis.toFixed(1)} °C</strong>
            <Typography component="span" variant="body2" color="text.secondary" ml={1}>
              (fans turn OFF below {(data.fan_temperature_threshold - data.fan_hysteresis).toFixed(1)} °C)
            </Typography>
          </Typography>
          <Slider
            value={data.fan_hysteresis}
            onChange={(_, v) => setData({ ...data, fan_hysteresis: v as number })}
            min={0.5}
            max={5}
            step={0.5}
            disabled={saving || !data.fan_control_enabled}
            valueLabelDisplay="auto"
            sx={{ maxWidth: 400 }}
          />
        </Box>

        <Divider sx={{ my: 2 }} />

        {/* ── Moisture Alarm ────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Moisture Alarm
        </Typography>
        <Typography variant="body2" color="text.secondary" mb={1}>
          The alarm fires when soil moisture drops below the threshold and stays there for the dwell period.
          The dwell prevents false alarms from noisy ADC readings. Calibrate the sensor in Hardware Settings first.
        </Typography>

        <BlockFormControlLabel
          control={
            <Checkbox
              name="moisture_alarm_enabled"
              checked={data.moisture_alarm_enabled}
              onChange={updateFormValue}
              disabled={saving}
            />
          }
          label="Enable moisture alarm"
        />

        <Box mt={1} mb={1}>
          <Typography gutterBottom>
            Alarm threshold: <strong>{data.moisture_alarm_threshold} %</strong>
          </Typography>
          <Slider
            value={data.moisture_alarm_threshold}
            onChange={(_, v) => setData({ ...data, moisture_alarm_threshold: v as number })}
            min={5}
            max={80}
            step={5}
            disabled={saving || !data.moisture_alarm_enabled}
            valueLabelDisplay="auto"
            sx={{ maxWidth: 400 }}
          />
        </Box>

        <Box mb={2}>
          <Typography gutterBottom>
            Dwell before alarm fires: <strong>{(data.moisture_alarm_dwell_ms / 1000).toFixed(0)} s</strong>
          </Typography>
          <Slider
            value={data.moisture_alarm_dwell_ms / 1000}
            onChange={(_, v) => setData({ ...data, moisture_alarm_dwell_ms: (v as number) * 1000 })}
            min={5}
            max={300}
            step={5}
            disabled={saving || !data.moisture_alarm_enabled}
            valueLabelDisplay="auto"
            sx={{ maxWidth: 400 }}
          />
        </Box>

        <Divider sx={{ my: 2 }} />

        <ButtonRow mt={1}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
            variant="contained"
            color="primary"
            onClick={saveData}
          >
            Save Automation
          </Button>
        </ButtonRow>
      </>
    );
  };

  return (
    <SectionContent title="Automation" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default GrowboxAutomationForm;
