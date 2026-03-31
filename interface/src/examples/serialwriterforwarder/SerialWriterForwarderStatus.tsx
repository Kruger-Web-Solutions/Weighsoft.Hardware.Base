import { FC } from 'react';
import { Chip, Paper, Table, TableBody, TableCell, TableRow, Typography } from '@mui/material';
import { SectionContent } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { useWs } from '../../utils/useWs';
import { ForwarderSourceProtocol, type SerialWriterForwarderData } from '../../types/serialWriterForwarder';

const SERIAL_WRITER_FORWARDER_WS_URL = WEB_SOCKET_ROOT + 'serialWriterForwarder';

const SerialWriterForwarderStatus: FC = () => {
  const { connected: wsConnected, data } = useWs<SerialWriterForwarderData>(
    SERIAL_WRITER_FORWARDER_WS_URL
  );

  const getProtocolLabel = (protocol: ForwarderSourceProtocol) => {
    return protocol === ForwarderSourceProtocol.HTTP ? 'HTTP Poll' : 'WebSocket Subscribe';
  };

  if (!data) {
    return (
      <SectionContent title="Status" titleGutter>
        <Typography>Connecting to device...</Typography>
      </SectionContent>
    );
  }

  return (
    <SectionContent title="Serial Writer Forwarder Status" titleGutter>
      <Paper>
        <Table>
          <TableBody>
            <TableRow>
              <TableCell>
                <strong>Service</strong>
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
                <strong>Source Connection</strong>
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
                <strong>WebSocket (UI)</strong>
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
                <strong>Source URL</strong>
              </TableCell>
              <TableCell sx={{ fontFamily: 'monospace', wordBreak: 'break-all' }}>
                {data.source_url || '—'}
              </TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>JSON Field</strong>
              </TableCell>
              <TableCell sx={{ fontFamily: 'monospace' }}>{data.json_field || '—'}</TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Lines Received</strong>
              </TableCell>
              <TableCell>{data.received_count ?? 0}</TableCell>
            </TableRow>

            <TableRow>
              <TableCell>
                <strong>Last Received</strong>
              </TableCell>
              <TableCell sx={{ fontFamily: 'monospace', wordBreak: 'break-all' }}>
                {data.last_received_line || '—'}
              </TableCell>
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

export default SerialWriterForwarderStatus;
