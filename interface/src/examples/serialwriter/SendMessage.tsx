import React, { FC, useState } from 'react';
import { Box, Card, CardContent, TextField, Button, Alert, Chip, Typography } from '@mui/material';
import SendIcon from '@mui/icons-material/Send';
import { SectionContent } from '../../components';
import { sendWriterMessage } from '../../api/serialWriter';

const RECENT_MAX = 8;

const SendMessage: FC = () => {
  const [msg, setMsg] = useState('');
  const [recent, setRecent] = useState<string[]>([]);
  const [lastResult, setLastResult] = useState<{ ok: boolean; text: string } | null>(null);
  const [sending, setSending] = useState(false);

  const send = async (text?: string) => {
    const payload = (text ?? msg).trim();
    if (payload.length === 0) return;
    setSending(true);
    try {
      await sendWriterMessage(payload);
      setLastResult({ ok: true, text: payload });
      setRecent((r) => {
        const next = [payload, ...r.filter((x) => x !== payload)];
        return next.slice(0, RECENT_MAX);
      });
      if (text === undefined) setMsg('');
    } catch (e: any) {
      setLastResult({ ok: false, text: e?.response?.data?.error || 'Failed to send' });
    } finally {
      setSending(false);
    }
  };

  return (
    <SectionContent title="Send Message" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Most scales respond to <code>P</code> to print weight, or <code>ZERO</code> to tare.
      </Alert>

      <Card sx={{ mb: 2 }}>
        <CardContent>
          <TextField
            fullWidth
            multiline
            minRows={1}
            label="Message"
            value={msg}
            onChange={(e) => setMsg(e.target.value)}
            onKeyDown={(e) => { if (e.key === 'Enter' && !e.shiftKey) { e.preventDefault(); send(); } }}
            placeholder="Type something… Enter to send · Shift+Enter for newline"
            sx={{ mb: 2 }}
          />
          <Box display="flex" justifyContent="flex-end">
            <Button variant="contained" startIcon={<SendIcon />} disabled={sending || msg.trim().length === 0} onClick={() => send()}>Send</Button>
          </Box>
        </CardContent>
      </Card>

      {lastResult && (
        <Alert severity={lastResult.ok ? 'success' : 'error'} sx={{ mb: 2 }}>
          {lastResult.ok ? 'Sent — ' : 'Failed — '} <code>{lastResult.text}</code>
        </Alert>
      )}

      {recent.length > 0 && (
        <Card>
          <CardContent>
            <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>Recent — click to send again</Typography>
            <Box display="flex" gap={1} flexWrap="wrap">
              {recent.map((r) => <Chip key={r} label={r} clickable onClick={() => send(r)} />)}
            </Box>
          </CardContent>
        </Card>
      )}
    </SectionContent>
  );
};

export default SendMessage;
