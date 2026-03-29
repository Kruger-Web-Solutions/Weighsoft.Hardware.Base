import React, { FC, useState, useEffect, useRef, useCallback } from 'react';
import {
  Typography, Box, Button, Chip, Checkbox, FormControlLabel, Divider
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, ButtonRow } from '../../components';
import { WEB_SOCKET_ROOT, AXIOS } from '../../api/endpoints';
import { useWs } from '../../utils';
import { useLayoutTitle } from '../../components';

export const REMOTE_WEIGHT_WS_URL = WEB_SOCKET_ROOT + "remoteWeight";
const REMOTE_WEIGHT_ENDPOINT = 'remoteWeight';

interface RemoteWeightData {
  weight: string;
  last_line: string;
  timestamp: number;
  enabled: boolean;
  display_enabled: boolean;
}

const RemoteWeightMonitor: FC = () => {
  useLayoutTitle("Remote Weight");

  const { connected, data } = useWs<RemoteWeightData>(REMOTE_WEIGHT_WS_URL);
  const [history, setHistory] = useState<Array<{ weight: string; line: string; time: Date }>>([]);
  const scrollRef = useRef<HTMLDivElement>(null);

  const [enabled, setEnabled] = useState(true);
  const [displayEnabled, setDisplayEnabled] = useState(true);
  const [saving, setSaving] = useState(false);
  const [loaded, setLoaded] = useState(false);

  useEffect(() => {
    if (data && !loaded) {
      setEnabled(data.enabled ?? true);
      setDisplayEnabled(data.display_enabled ?? true);
      setLoaded(true);
    }
  }, [data, loaded]);

  useEffect(() => {
    if (data?.weight) {
      setHistory((prev) => [...prev, {
        weight: data.weight,
        line: data.last_line,
        time: new Date()
      }].slice(-200));

      if (scrollRef.current) {
        scrollRef.current.scrollTop = scrollRef.current.scrollHeight;
      }
    }
  }, [data]);

  const saveConfig = useCallback(async () => {
    setSaving(true);
    try {
      await AXIOS.post(REMOTE_WEIGHT_ENDPOINT, { enabled, display_enabled: displayEnabled });
    } finally {
      setSaving(false);
    }
  }, [enabled, displayEnabled]);

  const formatTime = (d: Date) =>
    d.toLocaleTimeString('en-GB', { hour: '2-digit', minute: '2-digit', second: '2-digit' });

  return (
    <SectionContent title='Remote Weight Receiver' titleGutter>
      <Box sx={{ display: 'flex', flexDirection: 'column', gap: 1, mb: 2 }}>
        <FormControlLabel
          control={<Checkbox checked={enabled} onChange={(e) => setEnabled(e.target.checked)} />}
          label="Enable Remote Weight Receiver"
        />
        <FormControlLabel
          control={<Checkbox checked={displayEnabled} onChange={(e) => setDisplayEnabled(e.target.checked)} />}
          label="Enable TFT Display"
        />
        <ButtonRow>
          <Button
            startIcon={<SaveIcon />}
            variant="contained"
            size="small"
            onClick={saveConfig}
            disabled={saving}
          >
            Save
          </Button>
        </ButtonRow>
      </Box>

      <Divider sx={{ mb: 2 }} />

      <Box sx={{ display: 'flex', alignItems: 'center', gap: 2, mb: 2 }}>
        <Chip
          label={connected ? 'Connected' : 'Disconnected'}
          color={connected ? 'success' : 'error'}
          size="small"
        />
        {data?.weight && (
          <Typography variant="h4" color="primary" sx={{ fontFamily: 'monospace', fontWeight: 'bold' }}>
            {data.weight}
          </Typography>
        )}
      </Box>

      <Typography variant="body2" color="text.secondary" paragraph>
        Receiving weight data from the scale ESP via HTTP forwarding. Last 200 readings shown.
      </Typography>

      <Button onClick={() => setHistory([])} sx={{ mb: 2 }}>
        Clear History
      </Button>

      <Box
        ref={scrollRef}
        sx={{
          fontFamily: 'monospace',
          fontSize: '0.85rem',
          bgcolor: 'background.paper',
          p: 2,
          borderRadius: 1,
          height: 400,
          overflow: 'auto'
        }}
      >
        {history.length === 0 ? (
          <Typography color="text.secondary">Waiting for weight data from remote ESP...</Typography>
        ) : (
          history.map((entry, idx) => (
            <Box key={idx} sx={{ mb: 0.5 }}>
              <Typography component="span" variant="caption" color="text.secondary" sx={{ mr: 1 }}>
                [{formatTime(entry.time)}]
              </Typography>
              <Typography component="span" color="primary" sx={{ fontWeight: 'bold', mr: 1 }}>
                {entry.weight}
              </Typography>
              <Typography component="span" color="text.secondary">
                {entry.line}
              </Typography>
            </Box>
          ))
        )}
      </Box>
    </SectionContent>
  );
};

export default RemoteWeightMonitor;
