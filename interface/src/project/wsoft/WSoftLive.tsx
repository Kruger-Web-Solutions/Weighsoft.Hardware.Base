import { FC } from 'react';

import { Alert, AlertTitle, Box, Chip, Paper, Table, TableBody, TableCell, TableRow, Typography } from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { useWs } from '../../utils';

import WSoftSavePanel from './WSoftSavePanel';
import WSoftSwitches from './WSoftSwitches';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import { WSoftLink, WSoftNoticeLevel, WSoftStatus } from './types';

const LINK_LABEL: Record<WSoftLink, string> = {
  starting: 'Starting',
  no_signal: 'No signal',
  no_reply: 'No reply',
  alarm: 'DJB alarm',
  live: 'RS485 live'
};

const linkColor = (link: WSoftLink) => {
  if (link === 'live') return 'success';
  if (link === 'starting') return 'default';
  return 'error';
};

const NOTICE_SEVERITY: Record<WSoftNoticeLevel, 'error' | 'warning' | 'info'> = {
  stop: 'error',
  wait: 'warning',
  info: 'info'
};

// The big message over the weight: what stops a save right now, in the board screen's words
// (Rhyno, 9 Nov 2026: big and obvious, no looking around or clicking around).
const SaveNotice: FC<{ status: WSoftStatus }> = ({ status }) => {
  if (!status.notice_title || !status.notice_level) return null;
  return (
    <Alert
      severity={NOTICE_SEVERITY[status.notice_level] ?? 'error'}
      variant="filled"
      role="alert"
      sx={{ mb: 2, py: 2, '& .MuiAlert-icon': { fontSize: 48, alignItems: 'center' } }}
    >
      <AlertTitle sx={{ fontSize: '2rem', fontWeight: 700, lineHeight: 1.2, mb: 0.5 }}>{status.notice_title}</AlertTitle>
      <Typography sx={{ fontSize: '1.25rem' }}>{status.notice_detail}</Typography>
    </Alert>
  );
};

const WSoftLive: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);

  const content = (status: WSoftStatus) => (
    <>
      <SaveNotice status={status} />
      <Paper variant="outlined" sx={{ p: 3, mb: 2, textAlign: 'center' }}>
        <Typography variant="h2" component="div" sx={{ fontVariantNumeric: 'tabular-nums' }}>
          {status.weight_text ?? '----'}
        </Typography>
        <Typography variant="body2" color="text.secondary">
          {status.weight_g === null ? 'No weight' : `${status.weight_g} g`}
        </Typography>
        <Box sx={{ mt: 2, display: 'flex', gap: 1, justifyContent: 'center', flexWrap: 'wrap' }}>
          <Chip
            label={status.link === 'live' && status.source === 'rs232' ? 'RS232 live' : LINK_LABEL[status.link] ?? status.link}
            color={linkColor(status.link)}
            size="small"
          />
          <Chip
            label={status.stable ? 'Stable' : 'Not stable'}
            color={status.stable ? 'success' : 'default'}
            size="small"
          />
          <Chip
            label={status.for_trade ? 'Trade checks on' : 'Not for trade'}
            color={status.for_trade ? 'success' : 'error'}
            size="small"
          />
        </Box>
      </Paper>
      <WSoftSwitches />
      <WSoftSavePanel />
      <Table size="small">
        <TableBody>
          <TableRow>
            <TableCell><strong>Trade check</strong></TableCell>
            <TableCell>{status.trade_check}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Audit counter</strong></TableCell>
            <TableCell>{status.audit_counter}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Last trade sequence</strong></TableCell>
            <TableCell>{status.last_trade_seq || '-'}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Waiting for the Hub</strong></TableCell>
            <TableCell>{status.trade_queue}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Hub</strong></TableCell>
            <TableCell>{status.hub}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Screen</strong></TableCell>
            <TableCell>{status.display_ready ? 'Ready' : 'Not ready'}</TableCell>
          </TableRow>
        </TableBody>
      </Table>
    </>
  );

  return (
    <SectionContent title="W-Soft live" titleGutter>
      <MessageBox
        level="info"
        message="Nothing on this page is approved for trade: that needs NRCS approval and on-site verification."
        my={2}
      />
      {!connected || !data ? <FormLoader message="Connecting to WebSocket…" /> : content(data)}
    </SectionContent>
  );
};

export default WSoftLive;
