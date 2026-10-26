import { FC } from 'react';

import { Box, Chip, Paper, Table, TableBody, TableCell, TableRow, Typography } from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { useWs } from '../../utils';

import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import { WSoftLink, WSoftStatus } from './types';

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

const WSoftLive: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);

  const content = (status: WSoftStatus) => (
    <>
      <Paper variant="outlined" sx={{ p: 3, mb: 2, textAlign: 'center' }}>
        <Typography variant="h2" component="div" sx={{ fontVariantNumeric: 'tabular-nums' }}>
          {status.weight_text ?? '----'}
        </Typography>
        <Typography variant="body2" color="text.secondary">
          {status.weight_g === null ? 'No weight' : `${status.weight_g} g`}
        </Typography>
        <Box sx={{ mt: 2, display: 'flex', gap: 1, justifyContent: 'center', flexWrap: 'wrap' }}>
          <Chip label={LINK_LABEL[status.link] ?? status.link} color={linkColor(status.link)} size="small" />
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
      <Table size="small">
        <TableBody>
          <TableRow>
            <TableCell><strong>Approval</strong></TableCell>
            <TableCell>{status.approval_on ? 'ON' : 'OFF'} (switched with a USB cable only)</TableCell>
          </TableRow>
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
        message="Read-only. Nothing on this page is legal for trade: that needs NRCS approval and on-site verification."
        my={2}
      />
      {!connected || !data ? <FormLoader message="Connecting to WebSocket…" /> : content(data)}
    </SectionContent>
  );
};

export default WSoftLive;
