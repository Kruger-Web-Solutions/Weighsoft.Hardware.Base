import React, { FC, useEffect, useRef, useState } from 'react';
import { useSnackbar } from 'notistack';
import { Box, Card, CardContent, Typography, Alert, Chip, Button, IconButton, Tooltip, Dialog, DialogTitle, DialogContent, DialogActions } from '@mui/material';
import DeleteOutlineIcon from '@mui/icons-material/DeleteOutline';
import ContentCopyIcon from '@mui/icons-material/ContentCopy';
import { SectionContent } from '../../components';
import { readKnownWriters, forgetWriter } from '../../api/knownWriters';
import { KnownWriter } from '../../types/knownWriters';

// Phase B disconnect-notification rules:
// - Snackbar fires once per online↔offline transition.
// - Per-writer banner appears if a writer has been offline for ≥ this long.
// - Snackbar dedup window prevents spam during rapid flap.
const BANNER_OFFLINE_THRESHOLD_MS = 15_000;
const SNACK_DEDUP_MS              = 5_000;

// Format an elapsed-since-now value (milliseconds) into a short relative phrase.
// 0 means the event never happened in the current device session.
const formatElapsed = (msAgo: number): string => {
  if (!msAgo) return 'never';
  const sec = Math.floor(msAgo / 1000);
  if (sec < 5) return 'just now';
  if (sec < 60) return `${sec}s ago`;
  if (sec < 3600) return `${Math.floor(sec / 60)}m ago`;
  if (sec < 86400) return `${Math.floor(sec / 3600)}h ago`;
  return `${Math.floor(sec / 86400)}d ago`;
};

const Writers: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const [writers, setWriters] = useState<KnownWriter[] | null>(null);
  const [hostIp, setHostIp] = useState<string>('');
  const [pendingForget, setPendingForget] = useState<KnownWriter | null>(null);

  // Refs for transition tracking — survive re-renders without re-triggering effects.
  const previousOnlineRef = useRef<Map<string, boolean>>(new Map());
  const offlineSinceRef   = useRef<Map<string, number>>(new Map());
  const lastSnackRef      = useRef<Map<string, number>>(new Map());
  const [bannerWriters, setBannerWriters] = useState<KnownWriter[]>([]);

  useEffect(() => {
    setHostIp(window.location.hostname);
    const tick = () => readKnownWriters().then((r) => setWriters(r.data.writers)).catch(() => {});
    tick();
    const id = setInterval(tick, 2000);
    return () => clearInterval(id);
  }, []);

  // Detect online↔offline transitions on every poll, fire snackbars, refresh banner list.
  useEffect(() => {
    if (!writers) return;
    const now = Date.now();
    const previous = previousOnlineRef.current;

    for (const w of writers) {
      const wasOnline = previous.get(w.id);
      const isOnline = w.online;

      if (wasOnline === undefined) {
        // First sight — record initial state. If already offline, start the banner timer.
        previous.set(w.id, isOnline);
        if (!isOnline) offlineSinceRef.current.set(w.id, now);
        continue;
      }

      const lastSnack = lastSnackRef.current.get(w.id) ?? 0;
      const canSnack = now - lastSnack > SNACK_DEDUP_MS;

      if (wasOnline && !isOnline) {
        // online → offline
        if (canSnack) {
          enqueueSnackbar(`${w.friendly_name} disconnected — will auto-reconnect`, { variant: 'warning' });
          lastSnackRef.current.set(w.id, now);
        }
        offlineSinceRef.current.set(w.id, now);
      } else if (!wasOnline && isOnline) {
        // offline → online
        if (canSnack) {
          enqueueSnackbar(`${w.friendly_name} reconnected`, { variant: 'success' });
          lastSnackRef.current.set(w.id, now);
        }
        offlineSinceRef.current.delete(w.id);
      }

      previous.set(w.id, isOnline);
    }

    // Forget-cleanup: drop refs for writers no longer in the list.
    const liveIds = new Set(writers.map((w) => w.id));
    for (const id of Array.from(previous.keys())) {
      if (!liveIds.has(id)) {
        previous.delete(id);
        offlineSinceRef.current.delete(id);
        lastSnackRef.current.delete(id);
      }
    }

    // Recompute the banner list — writers offline ≥ threshold.
    const banner = writers.filter((w) => {
      if (w.online) return false;
      const since = offlineSinceRef.current.get(w.id);
      return since !== undefined && now - since > BANNER_OFFLINE_THRESHOLD_MS;
    });
    setBannerWriters(banner);
  }, [writers, enqueueSnackbar]);

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

      {bannerWriters.length > 0 && (
        <Alert severity="warning" sx={{ mb: 2 }}>
          {bannerWriters.length === 1 ? (
            <>Writer <strong>{bannerWriters[0].friendly_name}</strong> has been offline for a while &mdash; the device will keep trying to reconnect.</>
          ) : (
            <><strong>{bannerWriters.length} writers</strong> have been offline for a while &mdash; the devices will keep trying to reconnect.</>
          )}
        </Alert>
      )}

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
                    {w.online ? (
                      <>Connected now &middot; last message {formatElapsed(w.last_message_ms_ago)}</>
                    ) : (
                      <>Last seen {formatElapsed(w.last_seen_ms_ago)} &middot; last message {formatElapsed(w.last_message_ms_ago)}</>
                    )}
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
