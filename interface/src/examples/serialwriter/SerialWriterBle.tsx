import { FC } from 'react';
import {
  Alert,
  Box,
  Chip,
  Divider,
  Paper,
  Stack,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Typography,
} from '@mui/material';
import BluetoothIcon from '@mui/icons-material/Bluetooth';
import { SectionContent } from '../../components';

const BLE_SERVICE_UUID = '12350000-e8f2-537e-4f6c-d104768a1235';
const BLE_CHAR_UUID = '12350001-e8f2-537e-4f6c-d104768a1235';

const CHAR_FIELDS = [
  { key: 'baud_rate', type: 'uint32', description: 'Baud rate', writable: true },
  { key: 'data_bits', type: 'uint8', description: 'Data bits (5–8)', writable: true },
  { key: 'stop_bits', type: 'uint8', description: 'Stop bits (1–2)', writable: true },
  { key: 'parity', type: 'uint8', description: 'Parity (0=None, 1=Even, 2=Odd)', writable: true },
  { key: 'line_terminator', type: 'string', description: '"LF"|"CRLF"|"CR"|"NONE"', writable: true },
  { key: 'pending_line', type: 'string', description: 'Line to write (write trigger)', writable: true },
  { key: 'last_sent_line', type: 'string', description: 'Last line successfully sent', writable: false },
  { key: 'sent_count', type: 'uint32', description: 'Total lines sent', writable: false },
];

const SerialWriterBle: FC = () => (
  <SectionContent title="Bluetooth Low Energy Interface" titleGutter>
    <Alert severity="info" icon={<BluetoothIcon />} sx={{ mb: 2 }}>
      BLE must be enabled in <strong>BLE Settings</strong> before the Serial Writer BLE service starts. Use a generic
      BLE scanner (e.g. nRF Connect, LightBlue) or your own app to connect.
    </Alert>

    <Typography variant="h6" gutterBottom>
      Service UUID
    </Typography>
    <Paper variant="outlined" sx={{ p: 2, mb: 3, fontFamily: 'monospace', fontSize: '0.9rem', wordBreak: 'break-all' }}>
      {BLE_SERVICE_UUID}
    </Paper>

    <Divider sx={{ mb: 3 }} />

    <Box sx={{ mb: 4 }}>
      <Stack direction="row" alignItems="center" spacing={1} sx={{ mb: 1 }}>
        <BluetoothIcon fontSize="small" color="primary" />
        <Typography variant="subtitle1" fontWeight="bold">
          Serial Writer Characteristic
        </Typography>
        <Chip label="Read / Write / Notify" size="small" variant="outlined" />
      </Stack>

      <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
        Read current config and status. Write JSON to update config or trigger a serial write via{' '}
        <code>pending_line</code>. Notifications fire on every state change.
      </Typography>

      <Typography
        variant="caption"
        color="text.disabled"
        sx={{ fontFamily: 'monospace', display: 'block', mb: 1 }}
      >
        {BLE_CHAR_UUID}
      </Typography>

      <TableContainer component={Paper} variant="outlined">
        <Table size="small">
          <TableHead>
            <TableRow>
              <TableCell>
                <strong>Key</strong>
              </TableCell>
              <TableCell>
                <strong>Type</strong>
              </TableCell>
              <TableCell>
                <strong>Description</strong>
              </TableCell>
              <TableCell align="center">
                <strong>Writable</strong>
              </TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {CHAR_FIELDS.map((field) => (
              <TableRow key={field.key}>
                <TableCell sx={{ fontFamily: 'monospace' }}>{field.key}</TableCell>
                <TableCell sx={{ fontFamily: 'monospace', color: 'text.secondary' }}>{field.type}</TableCell>
                <TableCell>{field.description}</TableCell>
                <TableCell align="center">{field.writable ? '✓' : '—'}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      </TableContainer>
    </Box>

    <Divider sx={{ mb: 3 }} />

    <Typography variant="h6" gutterBottom>
      Usage Examples
    </Typography>

    <Typography variant="subtitle2" gutterBottom>
      Send a line to serial
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write: {"pending_line":"Hello world"}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      Change baud rate
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write: {"baud_rate":115200}`}
    </Paper>

    <Typography variant="subtitle2" gutterBottom>
      Change line terminator to CRLF
    </Typography>
    <Paper variant="outlined" sx={{ p: 1.5, mb: 2, fontFamily: 'monospace', fontSize: '0.85rem' }}>
      {`Write: {"line_terminator":"CRLF"}`}
    </Paper>

    <Alert severity="warning" sx={{ mt: 2 }}>
      Writing <code>pending_line</code> triggers an immediate serial write. The firmware appends the configured
      line terminator automatically. Make sure UART Mode is set to <strong>Serial Writer</strong> before sending.
    </Alert>
  </SectionContent>
);

export default SerialWriterBle;
