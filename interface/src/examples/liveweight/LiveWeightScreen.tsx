import { FC, useEffect, useMemo, useState } from 'react';

import { Box, Chip, Typography } from '@mui/material';

import { WEB_SOCKET_ROOT } from '../../api/endpoints';
import { FormLoader, SectionContent } from '../../components';
import { useWs } from '../../utils';

import { DEMO_LIVE_WEIGHT, LiveWeightState } from './types';

export const LIVE_WEIGHT_WS_URL = WEB_SOCKET_ROOT + 'liveWeight';

const FRESH_MS = 4000;

const LiveWeightScreen: FC = () => {
  const { connected, data } = useWs<LiveWeightState>(LIVE_WEIGHT_WS_URL);
  const [demoMode, setDemoMode] = useState(false);
  const [lastSeen, setLastSeen] = useState<number>(0);
  const [now, setNow] = useState(Date.now());

  useEffect(() => {
    if (!connected) {
      const t = window.setTimeout(() => setDemoMode(true), 2500);
      return () => window.clearTimeout(t);
    }
    setDemoMode(false);
    return undefined;
  }, [connected]);

  useEffect(() => {
    const id = window.setInterval(() => setNow(Date.now()), 500);
    return () => window.clearInterval(id);
  }, []);

  const state = demoMode ? DEMO_LIVE_WEIGHT : data;

  useEffect(() => {
    if (state?.timestamp) {
      setLastSeen(Date.now());
    }
  }, [state?.timestamp, state?.weight]);

  const fresh = useMemo(() => {
    if (demoMode) {
      return true;
    }
    return lastSeen > 0 && now - lastSeen < FRESH_MS;
  }, [demoMode, lastSeen, now]);

  if (!demoMode && !data && !connected) {
    return (
      <SectionContent title="Live Weight" titleGutter>
        <FormLoader message="Connecting…" />
      </SectionContent>
    );
  }

  const weight = state?.weight || '—';
  const unitHint = /\b(kg|g|lb|t)\b/i.exec(state?.last_line || '')?.[1] || '';

  return (
    <SectionContent title="Live Weight" titleGutter>
      <Box
        sx={{
          minHeight: { xs: 360, sm: 480 },
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          textAlign: 'center',
          px: 2
        }}
      >
        <Chip
          size="small"
          label={fresh ? 'Live' : 'Waiting'}
          color={fresh ? 'success' : 'default'}
          sx={{ mb: 3 }}
        />

        <Typography
          sx={{
            fontFamily: 'IBM Plex Mono, Consolas, monospace',
            fontWeight: 700,
            fontSize: { xs: '4.5rem', sm: '7rem' },
            lineHeight: 1,
            letterSpacing: '-0.03em'
          }}
        >
          {weight}
        </Typography>

        {unitHint ? (
          <Typography variant="h5" color="text.secondary" sx={{ mt: 1, letterSpacing: '0.08em' }}>
            {unitHint.toUpperCase()}
          </Typography>
        ) : null}

        <Typography variant="body2" color="text.secondary" sx={{ mt: 4, maxWidth: 420 }}>
          {fresh ? 'Weight updating' : 'Waiting for the next reading'}
        </Typography>
      </Box>
    </SectionContent>
  );
};

export default LiveWeightScreen;
