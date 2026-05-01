import React, { FC, useState } from 'react';
import { Card, CardContent, TextField, Button, Alert, Typography, Box } from '@mui/material';
import SendIcon from '@mui/icons-material/Send';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { SectionContent } from '../../components';
import { sendWriterMessage, SERIALW_SEND_PATH } from '../../api/serialWriter';

const SendViaWeb: FC = () => {
  const [msg, setMsg] = useState('');
  const [result, setResult] = useState<{ ok: boolean; text: string } | null>(null);
  const [sending, setSending] = useState(false);

  const send = async () => {
    if (msg.trim().length === 0) return;
    setSending(true);
    try {
      await sendWriterMessage(msg);
      setResult({ ok: true, text: msg });
    } catch (e: any) {
      setResult({ ok: false, text: e?.response?.data?.error || 'Failed' });
    } finally {
      setSending(false);
    }
  };

  const curlExample = `curl -X POST http://<device-ip>${SERIALW_SEND_PATH} \\\n  -H "Content-Type: application/json" \\\n  -d '{"data":"P"}'`;

  return (
    <SectionContent title="Send via Web (HTTP)" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>POST a message and the device writes it to the serial port.</Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <Typography variant="caption" color="text.secondary">Endpoint</Typography>
          <Typography sx={{ fontFamily: 'monospace', mb: 2 }}>
            <strong>POST</strong> {SERIALW_SEND_PATH}<br />
            Body: <code>{`{"data":"..."}`}</code>
          </Typography>

          <Typography variant="subtitle2" gutterBottom>Try it</Typography>
          <TextField fullWidth label="Message" value={msg} onChange={(e) => setMsg(e.target.value)} sx={{ mb: 2 }} />
          <Box display="flex" justifyContent="flex-end">
            <Button
              variant="contained"
              startIcon={<SendIcon />}
              disabled={sending || msg.trim().length === 0}
              onClick={send}
            >
              Send
            </Button>
          </Box>
        </CardContent>
      </Card>

      {result && (
        <Alert severity={result.ok ? 'success' : 'error'} sx={{ mb: 2 }}>
          {result.ok ? `Sent: ${result.text}` : result.text}
        </Alert>
      )}

      <Card>
        <CardContent>
          <Typography variant="subtitle2" gutterBottom>Example with curl</Typography>
          <Box position="relative">
            <pre style={{ background: '#f5f5f5', padding: 12, fontSize: 12, overflowX: 'auto', margin: 0 }}>{curlExample}</pre>
            <Button
              size="small"
              startIcon={<ContentCopyIcon />}
              sx={{ position: 'absolute', top: 4, right: 4 }}
              onClick={() => navigator.clipboard?.writeText(curlExample)}
            >
              Copy
            </Button>
          </Box>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default SendViaWeb;
