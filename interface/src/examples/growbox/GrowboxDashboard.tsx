import { FC } from 'react';

import {
  Box,
  Chip,
  Divider,
  Grid,
  Switch,
  Tooltip,
  Typography
} from '@mui/material';
import { alpha } from '@mui/material/styles';
import LightModeIcon from '@mui/icons-material/LightMode';
import AirIcon from '@mui/icons-material/Air';
import ThermostatIcon from '@mui/icons-material/Thermostat';
import WaterDropIcon from '@mui/icons-material/Opacity';
import WifiIcon from '@mui/icons-material/Wifi';
import WifiOffIcon from '@mui/icons-material/WifiOff';
import AccessTimeIcon from '@mui/icons-material/AccessTime';
import WarningAmberIcon from '@mui/icons-material/WarningAmber';

import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { BlockFormControlLabel, FormLoader, MessageBox, SectionContent } from '../../components';
import { updateValue, useWs } from '../../utils';

import { GrowboxState } from './types';

export const GROWBOX_WEBSOCKET_URL = WEB_SOCKET_ROOT + 'growbox';

function formatUptime(ms: number): string {
  const totalSeconds = Math.floor(ms / 1000);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours}h ${minutes}m ${seconds}s`;
}

function ModeBadge({ mode }: { mode: string }) {
  const color: 'info' | 'success' | 'default' =
    mode === 'schedule' ? 'info' : mode === 'auto' ? 'success' : 'default';
  return <Chip label={mode} size="small" color={color} />;
}

const GrowboxDashboard: FC = () => {
  const { connected, updateData, data } = useWs<GrowboxState>(GROWBOX_WEBSOCKET_URL);
  const updateFormValue = updateValue(updateData);

  const content = () => {
    if (!connected || !data) {
      return <FormLoader message="Connecting to Growbox…" />;
    }

    return (
      <>
        {/* ── Status bar ───────────────────────────────────────── */}
        <Box display="flex" gap={1} flexWrap="wrap" mb={2}>
          <Chip
            icon={data.wifi_connected ? <WifiIcon /> : <WifiOffIcon />}
            label={data.wifi_connected ? 'WiFi Connected' : 'WiFi Offline'}
            color={data.wifi_connected ? 'success' : 'warning'}
            size="small"
          />
          <Chip
            icon={<AccessTimeIcon />}
            label={data.ntp_synced ? 'Time Synced' : 'Time Not Synced'}
            color={data.ntp_synced ? 'success' : 'warning'}
            size="small"
          />
          <Chip label={`Uptime: ${formatUptime(data.uptime_ms)}`} size="small" variant="outlined" />
          <Chip label={`Boots: ${data.boot_count}`} size="small" variant="outlined" />
        </Box>

        {!data.ntp_synced && (
          <MessageBox
            level="warning"
            message="Time is not synced. Light scheduling requires NTP. Configure your timezone in NTP Settings."
            my={1}
          />
        )}

        {data.moisture_alarm_active && !data.moisture_alarm_acknowledged && (
          <MessageBox
            level="error"
            message={`Moisture alarm active! ${data.moisture_alarm_reason || 'Soil moisture is below threshold.'}`}
            my={1}
          />
        )}

        {/* ── Outputs ──────────────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Outputs
        </Typography>

        <Grid container spacing={2} mb={2}>
          {/* Lights */}
          <Grid item xs={12} sm={4}>
            <Box
              sx={(theme) => ({
                p: 2,
                border: 1,
                borderColor: data.lights_on ? 'warning.main' : 'divider',
                borderRadius: 1,
                bgcolor: data.lights_on ? alpha(theme.palette.warning.main, 0.08) : 'background.paper'
              })}
            >
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <LightModeIcon color={data.lights_on ? 'warning' : 'disabled'} />
                <Typography variant="subtitle2">Lights</Typography>
                <Box ml="auto">
                  <ModeBadge mode={data.lights_mode} />
                </Box>
              </Box>
              <BlockFormControlLabel
                control={
                  <Switch
                    name="lights_on"
                    checked={data.lights_on}
                    onChange={updateFormValue}
                    color="warning"
                  />
                }
                label={data.lights_on ? 'ON' : 'OFF'}
              />
              <Typography variant="caption" color="text.secondary">
                {data.lights_reason || 'manual control'}
              </Typography>
            </Box>
          </Grid>

          {/* Fan In */}
          <Grid item xs={12} sm={4}>
            <Box
              sx={(theme) => ({
                p: 2,
                border: 1,
                borderColor: data.fan_in_on ? 'primary.main' : 'divider',
                borderRadius: 1,
                bgcolor: data.fan_in_on ? alpha(theme.palette.primary.main, 0.08) : 'background.paper'
              })}
            >
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <AirIcon color={data.fan_in_on ? 'primary' : 'disabled'} />
                <Typography variant="subtitle2">Fan In</Typography>
                <Box ml="auto">
                  <ModeBadge mode={data.fan_in_mode} />
                </Box>
              </Box>
              <BlockFormControlLabel
                control={
                  <Switch
                    name="fan_in_on"
                    checked={data.fan_in_on}
                    onChange={updateFormValue}
                    color="primary"
                  />
                }
                label={data.fan_in_on ? 'ON' : 'OFF'}
              />
              <Typography variant="caption" color="text.secondary">
                {data.fan_in_reason || 'manual control'}
              </Typography>
            </Box>
          </Grid>

          {/* Fan Out */}
          <Grid item xs={12} sm={4}>
            <Box
              sx={(theme) => ({
                p: 2,
                border: 1,
                borderColor: data.fan_out_on ? 'primary.main' : 'divider',
                borderRadius: 1,
                bgcolor: data.fan_out_on ? alpha(theme.palette.primary.main, 0.08) : 'background.paper'
              })}
            >
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <AirIcon color={data.fan_out_on ? 'primary' : 'disabled'} />
                <Typography variant="subtitle2">Fan Out</Typography>
                <Box ml="auto">
                  <ModeBadge mode={data.fan_out_mode} />
                </Box>
              </Box>
              <BlockFormControlLabel
                control={
                  <Switch
                    name="fan_out_on"
                    checked={data.fan_out_on}
                    onChange={updateFormValue}
                    color="primary"
                  />
                }
                label={data.fan_out_on ? 'ON' : 'OFF'}
              />
              <Typography variant="caption" color="text.secondary">
                {data.fan_out_reason || 'manual control'}
              </Typography>
            </Box>
          </Grid>
        </Grid>

        <Divider sx={{ my: 2 }} />

        {/* ── Sensors ──────────────────────────────────────────── */}
        <Typography variant="h6" gutterBottom>
          Sensors
        </Typography>

        <Grid container spacing={2} mb={2}>
          {/* Inside (SHT20) */}
          <Grid item xs={12} sm={4}>
            <Box sx={{ p: 2, border: 1, borderColor: 'divider', borderRadius: 1 }}>
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <ThermostatIcon color={data.inside_sensor_fault ? 'error' : 'action'} />
                <Typography variant="subtitle2">Inside (SHT20)</Typography>
                {data.inside_sensor_fault && (
                  <Tooltip title="Sensor fault — check SHT20 wiring">
                    <WarningAmberIcon color="error" fontSize="small" />
                  </Tooltip>
                )}
              </Box>
              <Typography variant="h5" color={data.inside_sensor_fault ? 'error.main' : 'text.primary'}>
                {data.inside_sensor_fault ? '—' : `${data.inside_temperature.toFixed(1)} °C`}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                {data.inside_sensor_fault ? 'Fault — check I2C wiring' : `${data.inside_humidity.toFixed(1)} % RH`}
              </Typography>
            </Box>
          </Grid>

          {/* Outside (DHT22) */}
          <Grid item xs={12} sm={4}>
            <Box sx={{ p: 2, border: 1, borderColor: 'divider', borderRadius: 1 }}>
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <ThermostatIcon color={data.outside_sensor_fault ? 'error' : 'action'} />
                <Typography variant="subtitle2">Outside (DHT22)</Typography>
                {data.outside_sensor_fault && (
                  <Tooltip title="Sensor fault — check DHT22 wiring and 10K pull-up resistor">
                    <WarningAmberIcon color="error" fontSize="small" />
                  </Tooltip>
                )}
              </Box>
              <Typography variant="h5" color={data.outside_sensor_fault ? 'error.main' : 'text.primary'}>
                {data.outside_sensor_fault ? '—' : `${data.outside_temperature.toFixed(1)} °C`}
              </Typography>
              <Typography variant="body2" color="text.secondary">
                {data.outside_sensor_fault ? 'Fault — check wiring' : `${data.outside_humidity.toFixed(1)} % RH`}
              </Typography>
            </Box>
          </Grid>

          {/* Moisture */}
          <Grid item xs={12} sm={4}>
            <Box
              sx={(theme) => ({
                p: 2,
                border: 1,
                borderColor: data.moisture_alarm_active ? 'error.main' : 'divider',
                borderRadius: 1,
                bgcolor: data.moisture_alarm_active ? alpha(theme.palette.error.main, 0.08) : 'background.paper'
              })}
            >
              <Box display="flex" alignItems="center" gap={1} mb={1}>
                <WaterDropIcon
                  color={
                    data.moisture_sensor_fault ? 'error' : data.moisture_alarm_active ? 'error' : 'action'
                  }
                />
                <Typography variant="subtitle2">Moisture</Typography>
                {data.moisture_sensor_fault && (
                  <Tooltip title="Sensor fault — check moisture probe ADC wiring">
                    <WarningAmberIcon color="error" fontSize="small" />
                  </Tooltip>
                )}
              </Box>
              {!data.moisture_calibrated ? (
                <Typography variant="body2" color="warning.main">
                  Not calibrated — go to Settings to calibrate
                </Typography>
              ) : data.moisture_sensor_fault ? (
                <Typography variant="h5" color="error.main">
                  —
                </Typography>
              ) : (
                <>
                  <Typography variant="h5" color={data.moisture_alarm_active ? 'error.main' : 'text.primary'}>
                    {data.moisture_percent.toFixed(0)} %
                  </Typography>
                  <Typography variant="body2" color="text.secondary">
                    Raw ADC: {data.moisture_raw}
                  </Typography>
                </>
              )}
              {data.moisture_alarm_active && (
                <Box mt={1}>
                  <BlockFormControlLabel
                    control={
                      <Switch
                        name="moisture_alarm_acknowledged"
                        checked={data.moisture_alarm_acknowledged}
                        onChange={updateFormValue}
                        color="warning"
                        size="small"
                      />
                    }
                    label="Acknowledge alarm"
                  />
                </Box>
              )}
            </Box>
          </Grid>
        </Grid>
      </>
    );
  };

  return (
    <SectionContent title="Dashboard" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default GrowboxDashboard;
