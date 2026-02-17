import { FC, useEffect } from 'react';
import { Box, Chip, Paper, Table, TableBody, TableCell, TableRow, Typography } from '@mui/material';
import { ForwardProtocol, type WeightForwarderData } from '../../types/weightForwarder';
import { useWs } from '../../utils/useWs';
import { SectionContent } from '../../components';

const WEIGHT_FORWARDER_SOCKET_PATH = '/ws/weightForwarder';

const WeightForwarderStatus: FC = () => {
  const { connected: wsConnected, data } = useWs<WeightForwarderData>(WEIGHT_FORWARDER_SOCKET_PATH);

  const getProtocolLabel = (protocol: ForwardProtocol) => {
    switch (protocol) {
      case ForwardProtocol.HTTP:
        return 'HTTP POST';
      case ForwardProtocol.WS:
        return 'WebSocket Client';
      case ForwardProtocol.MQTT:
        return 'MQTT';
      case ForwardProtocol.BLE:
        return 'BLE Client';
      default:
        return 'Unknown';
    }
  };

  const getLastForwardTime = () => {
    if (!data || data.last_forward_time === 0) return 'Never';
    const date = new Date(data.last_forward_time);
    return date.toLocaleTimeString();
  };

  const getTargetInfo = () => {
    if (!data) return 'Not configured';
    switch (data.protocol) {
      case ForwardProtocol.HTTP:
        return data.target_url || 'Not configured';
      case ForwardProtocol.WS:
        return data.ws_url || 'Not configured';
      case ForwardProtocol.MQTT:
        return data.mqtt_topic || 'Not configured';
      case ForwardProtocol.BLE:
        return data.ble_service_uuid || 'Not configured';
      default:
        return 'Unknown';
    }
  };

  if (!data) {
    return (
      <SectionContent title="Status" titleGutter>
        <Typography>Connecting to device...</Typography>
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Weight Stream Forwarder Status" titleGutter>

      <Paper>
        <Table>
          <TableBody>
            <TableRow>
              <TableCell>
                <strong>Service Status</strong>
              </TableCell>
              <TableCell>
                <Chip
                  label={data.enabled ? 'Enabled' : 'Disabled'}
                  color={data.enabled ? 'success' : 'default'}
                  size="small"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Connection Status</strong>
              </TableCell>
              <TableCell>
                <Chip
                  label={data.connected ? 'Connected' : 'Disconnected'}
                  color={data.connected ? 'success' : 'error'}
                  size="small"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>WebSocket Status</strong>
              </TableCell>
              <TableCell>
                <Chip
                  label={wsConnected ? 'Connected' : 'Disconnected'}
                  color={wsConnected ? 'success' : 'warning'}
                  size="small"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Protocol</strong>
              </TableCell>
              <TableCell>{getProtocolLabel(data.protocol)}</TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Target</strong>
              </TableCell>
              <TableCell>{getTargetInfo()}</TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Display Mode</strong>
              </TableCell>
              <TableCell>
                <Chip
                  label={data.display_mode ? 'LCD Format (line1/line2)' : 'Standard Format'}
                  size="small"
                  variant="outlined"
                />
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Last Forward Time</strong>
              </TableCell>
              <TableCell>{getLastForwardTime()}</TableCell>
            </TableRow>

            {data.last_error && (
              <TableRow>
                <TableCell>
                  <strong>Last Error</strong>
                </TableCell>
                <TableCell>
                  <Typography color="error">{data.last_error}</Typography>
                </TableCell>
              </TableRow>
            )}
          </TableBody>
        </Table>
      </Paper>
    </SectionContent>
  );
};

export default WeightForwarderStatus;
