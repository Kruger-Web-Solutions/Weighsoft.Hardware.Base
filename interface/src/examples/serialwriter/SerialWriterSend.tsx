import { FC, useState } from 'react';
import {
  Alert,
  Box,
  Button,
  Chip,
  Paper,
  Table,
  TableBody,
  TableCell,
  TableRow,
  TextField,
  Typography,
} from '@mui/material';
import SendIcon from '@mui/icons-material/Send';
import { SectionContent } from '../../components';
import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { sendSerialWriterLine } from '../../api/serialWriter';
import { useWs } from '../../utils/useWs';
import type { SerialWriterData } from '../../types/serialWriter';

const SERIAL_WRITER_WS_URL = WEB_SOCKET_ROOT + 'serialWriter';

const SerialWriterSend: FC = () => {
  const { connected: wsConnected, data } = useWs<SerialWriterData>(SERIAL_WRITER_WS_URL);
  const [inputLine, setInputLine] = useState('');
  const [sending, setSending] = useState(false);
  const [lastError, setLastError] = useState<string | null>(null);

  const handleSend = async () => {
    const line = inputLine.trim();
    if (!line) return;
    setSending(true);
    setLastError(null);
    try {
      await sendSerialWriterLine({ pending_line: line });
      setInputLine('');
    } catch (e: any) {
      setLastError(e?.message ?? 'Send failed');
    } finally {
      setSending(false);
    }
  };

  const handleKeyDown = (e: React.KeyboardEvent) => {
    if (e.key === 'Enter' && !e.shiftKey) {
      e.preventDefault();
      handleSend();
    }
  };

  return (
    <SectionContent title="Send to Serial" titleGutter>
      <Box display="flex" alignItems="center" gap={1} mb={2}>
        <Chip
          label={wsConnected ? 'Connected' : 'Disconnected'}
          color={wsConnected ? 'success' : 'warning'}
          size="small"
        />
        <Typography variant="caption" color="text.secondary">
          Live status via WebSocket
        </Typography>
      </Box>

      {lastError && (
        <Alert severity="error" sx={{ mb: 2 }} onClose={() => setLastError(null)}>
          {lastError}
        </Alert>
      )}

      <Box sx={{ display: 'flex', gap: 1, mb: 3 }}>
        <TextField
          fullWidth
          label="Line to send"
          value={inputLine}
          onChange={(e) => setInputLine(e.target.value)}
          onKeyDown={handleKeyDown}
          disabled={sending}
          placeholder="Type text and press Enter or click Send"
          helperText="Line terminator is appended automatically (see Configuration)"
          autoComplete="off"
        />
        <Button
          variant="contained"
          startIcon={<SendIcon />}
          onClick={handleSend}
          disabled={sending || !inputLine.trim()}
          sx={{ minWidth: 110, alignSelf: 'flex-start', mt: '8px' }}
        >
          Send
        </Button>
      </Box>

      {data && (
        <Paper variant="outlined">
          <Table size="small">
            <TableBody>
              <TableRow>
                <TableCell>
                  <strong>Last Sent</strong>
                </TableCell>
                <TableCell sx={{ fontFamily: 'monospace' }}>
                  {data.last_sent_line || '—'}
                </TableCell>
              </TableRow>
              <TableRow>
                <TableCell>
                  <strong>Sent Count</strong>
                </TableCell>
                <TableCell>{data.sent_count ?? 0}</TableCell>
              </TableRow>
              <TableRow>
                <TableCell>
                  <strong>Baud Rate</strong>
                </TableCell>
                <TableCell>{data.baud_rate ?? 9600}</TableCell>
              </TableRow>
              <TableRow>
                <TableCell>
                  <strong>Line Terminator</strong>
                </TableCell>
                <TableCell sx={{ fontFamily: 'monospace' }}>{data.line_terminator ?? 'LF'}</TableCell>
              </TableRow>
            </TableBody>
          </Table>
        </Paper>
      )}
    </SectionContent>
  );
};

export default SerialWriterSend;
