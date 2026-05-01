import React, { FC, useEffect, useState } from 'react';
import { Box, Card, CardContent, Typography, Alert, Chip, Button, IconButton, Tooltip, Dialog, DialogTitle, DialogContent, DialogActions } from '@mui/material';
import DeleteOutlineIcon from '@mui/icons-material/DeleteOutline';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { SectionContent } from '../../components';
import { readKnownWriters, forgetWriter } from '../../api/knownWriters';
import { KnownWriter } from '../../types/knownWriters';

const formatRelative = (ts: number): string => {
  if (!ts) return '—';
  const elapsed = Math.floor((Date.now() - ts) / 1000);
  if (elapsed < 5) return 'just now';
  if (elapsed < 60) return `${elapsed}s ago`;
  if (elapsed < 3600) return `${Math.floor(elapsed / 60)}m ago`;
  if (elapsed < 86400) return `${Math.floor(elapsed / 3600)}h ago`;
  return `${Math.floor(elapsed / 86400)}d ago`;
};

const Writers: FC = () => {
  const [writers, setWriters] = useState<KnownWriter[] | null>(null);
  const [hostIp, setHostIp] = useState<string>('');
  const [pendingForget, setPendingForget] = useState<KnownWriter | null>(null);

  useEffect(() => {
    setHostIp(window.location.hostname);
    const tick = () => readKnownWriters().then((r) => setWriters(r.data.writers)).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  const confirmForget = async () => {
    if (!pendingForget) return;
    try {
      await forgetWriter(pendingForget.id);
      setWriters((prev) => prev ? prev.filter((w) => w.id !== pendingForget.id) : prev);
    } finally {
      setPendingForget(null);
    }
  };

  return (
    <SectionContent title="Writers" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        To add a Writer, open that device's UI, go to <strong>Serial &rarr; Settings</strong>, and paste this Reader's address:&nbsp;
        <code>{hostIp}</code>
        <Tooltip title="Copy address">
          <IconButton size="small" onClick={() => navigator.clipboard?.writeText(hostIp)}>
            <ContentCopyIcon fontSize="small" />
          </IconButton>
        </Tooltip>
      </Alert>

      {!writers ? (
        <Typography color="text.secondary">Loading…</Typography>
      ) : writers.length === 0 ? (
        <Card><CardContent>
          <Typography color="text.secondary">
            No Writers connected yet. Set up a Writer device, point it at this Reader, and it&apos;ll appear here.
          </Typography>
        </CardContent></Card>
      ) : (
        writers.map((w) => (
          <Card key={w.id} sx={{ mb: 1 }}>
            <CardContent>
              <Box display="flex" alignItems="center" justifyContent="space-between" gap={2}>
                <Box flex={1}>
                  <Box display="flex" alignItems="center" gap={1}>
                    <Chip
                      size="small"
                      color={w.online ? 'success' : 'default'}
                      label={w.online ? 'online' : 'offline'}
                    />
                    <Typography variant="h6">{w.friendly_name}</Typography>
                    <Typography variant="caption" color="text.secondary">{w.ip}</Typography>
                  </Box>
                  <Typography variant="body2" color="text.secondary" sx={{ mt: 0.5 }}>
                    Last seen {formatRelative(w.last_seen_at)} &middot; last message {formatRelative(w.last_message_at)}
                  </Typography>
                  <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5, wordBreak: 'break-all' }}>
                    {w.last_message || '(no messages received yet)'}
                  </Typography>
                </Box>
                <Tooltip title="Forget this Writer">
                  <IconButton color="error" onClick={() => setPendingForget(w)}>
                    <DeleteOutlineIcon />
                  </IconButton>
                </Tooltip>
              </Box>
            </CardContent>
          </Card>
        ))
      )}

      <Dialog open={pendingForget !== null} onClose={() => setPendingForget(null)}>
        <DialogTitle>Forget {pendingForget?.friendly_name}?</DialogTitle>
        <DialogContent>
          <Typography>
            This Writer will be removed from the list permanently. It can re-add itself by reconnecting.
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setPendingForget(null)}>Cancel</Button>
          <Button color="error" variant="contained" onClick={confirmForget}>Forget</Button>
        </DialogActions>
      </Dialog>
    </SectionContent>
  );
};

export default Writers;
