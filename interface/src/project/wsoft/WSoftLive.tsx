import { FC, useCallback, useEffect, useState } from 'react';
import { AxiosError } from 'axios';

import {
  Alert, Box, Button, Chip, Paper, Table, TableBody, TableCell, TableHead, TableRow, Typography
} from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { useWs } from '../../utils';

import * as WSoftApi from './api';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import { WSoftLink, WSoftSavedWeighing, WSoftSaveResult, WSoftStatus } from './types';

const LINK_LABEL: Record<WSoftLink, string> = {
  starting: 'Starting',
  no_signal: 'No signal',
  no_reply: 'No reply',
  alarm: 'DJB alarm',
  live: 'RS485 live'
};

// Same states as the screen's save line (lib/wsoft_core/src/save_logic.cpp save_check_text).
const SAVE_REASON: Record<string, string> = {
  ok: 'Ready to save',
  moving: 'Moving: wait until the weight is still',
  alarm: 'DJB alarm',
  no_signal: 'No signal from the DJB',
  stale: 'No reply from the DJB',
  empty: 'Scale is empty',
  saved: 'This load is saved. Take it off for the next one',
  unit: 'The DJB unit cannot be sent as kg',
  no_clock: 'No clock yet (it comes from the Hub or the internet)',
  queue_full: 'Save queue full: waiting for the Hub',
  busy: 'The scale did not answer. Try again'
};

const reasonText = (reason?: string) => (reason ? SAVE_REASON[reason] ?? reason : 'Not saved');

const linkColor = (link: WSoftLink) => {
  if (link === 'live') return 'success';
  if (link === 'starting') return 'default';
  return 'error';
};

const WSoftLive: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);
  const [saving, setSaving] = useState(false);
  const [saveMessage, setSaveMessage] = useState<{ ok: boolean; text: string }>();
  const [recent, setRecent] = useState<WSoftSavedWeighing[]>();

  const lastSeq = data?.save_last_seq;
  const queue = data?.save_queue;

  const loadRecent = useCallback(async () => {
    try {
      const { data: list } = await WSoftApi.readSavedWeighings();
      setRecent(list.recent);
    } catch {
      // The list is a convenience; the live status above still shows the last save.
    }
  }, []);

  // Refresh the list when a new weighing is saved or the Hub takes one.
  useEffect(() => {
    if (lastSeq !== undefined) loadRecent();
  }, [lastSeq, queue, loadRecent]);

  const save = async () => {
    setSaving(true);
    setSaveMessage(undefined);
    try {
      const { data: result } = await WSoftApi.saveWeighing();
      setSaveMessage({ ok: true, text: `Saved #${result.seq}${result.weight_text ? `: ${result.weight_text}` : ''}` });
      loadRecent();
    } catch (error: any) {
      const body = (error as AxiosError<WSoftSaveResult>).response?.data;
      setSaveMessage({ ok: false, text: `Not saved: ${reasonText(body?.reason)}` });
    } finally {
      setSaving(false);
    }
  };

  const savePanel = (status: WSoftStatus) => (
    <Paper variant="outlined" sx={{ p: 2, mb: 2 }}>
      <Box sx={{ display: 'flex', gap: 2, alignItems: 'center', flexWrap: 'wrap' }}>
        <Button
          size="large"
          variant="contained"
          color="success"
          startIcon={<SaveIcon />}
          disabled={saving || !status.save_can}
          onClick={save}
        >
          Save
        </Button>
        <Box>
          <Typography variant="body1">
            {status.save_reason === 'saved' && status.save_last_seq
              ? `Saved #${status.save_last_seq}${status.save_last_text ? `: ${status.save_last_text}` : ''}`
              : reasonText(status.save_reason)}
          </Typography>
          <Typography variant="body2" color="text.secondary">
            {status.save_auto ? `Auto-save on: after ${status.save_auto_s} s still` : 'Auto-save off (W-Soft › Settings)'}
            {' · '}
            {status.save_queue === 0 ? 'All sent to the Hub' : `${status.save_queue} waiting for the Hub`}
          </Typography>
        </Box>
      </Box>
      {saveMessage && (
        <Alert severity={saveMessage.ok ? 'success' : 'warning'} sx={{ mt: 2 }}>
          {saveMessage.text}
        </Alert>
      )}
      {recent && recent.length > 0 && (
        <Table size="small" sx={{ mt: 2 }}>
          <TableHead>
            <TableRow>
              <TableCell>#</TableCell>
              <TableCell>Weight</TableCell>
              <TableCell>Time (UTC)</TableCell>
              <TableCell>How</TableCell>
              <TableCell>Hub</TableCell>
            </TableRow>
          </TableHead>
          <TableBody>
            {recent.map((w) => (
              <TableRow key={w.seq}>
                <TableCell>{w.seq}</TableCell>
                <TableCell>{w.weight_text}</TableCell>
                <TableCell>{w.at ? w.at.replace('T', ' ').replace('Z', '') : '-'}</TableCell>
                <TableCell>{w.auto_saved ? 'Auto' : 'Button'}{w.trade ? ' (trade)' : ''}</TableCell>
                <TableCell>{w.delivered ? 'Sent' : 'Waiting'}</TableCell>
              </TableRow>
            ))}
          </TableBody>
        </Table>
      )}
    </Paper>
  );

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
      {savePanel(status)}
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
            <TableCell><strong>Trade weights waiting for the Hub</strong></TableCell>
            <TableCell>{status.trade_queue}</TableCell>
          </TableRow>
          <TableRow>
            <TableCell><strong>Last Hub answer for a weighing</strong></TableCell>
            <TableCell>{status.save_hub_status || '-'}</TableCell>
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
        message="Nothing on this page is legal for trade: that needs NRCS approval and on-site verification."
        my={2}
      />
      {!connected || !data ? <FormLoader message="Connecting to WebSocket…" /> : content(data)}
    </SectionContent>
  );
};

export default WSoftLive;
