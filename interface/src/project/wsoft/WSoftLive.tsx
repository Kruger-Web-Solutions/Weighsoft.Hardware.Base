import { FC, MouseEvent, useEffect, useRef, useState } from 'react';

import { Box, Table, TableBody, TableCell, TableRow } from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';
import { useWs } from '../../utils';

import * as WSoftApi from './api';
import WSoftSavePanel from './WSoftSavePanel';
import WSoftSwitches from './WSoftSwitches';
import { WSOFT_STATUS_WEBSOCKET_URL } from './api';
import { paintPicture, tapPoint, toRgba, PICTURE_H, PICTURE_W } from './screen';
import { WSoftStatus } from './types';

const REFRESH_MS = 250;
const LOST_AFTER = 8;  // misses in a row (about 2 s) before the page says the board is gone

// The board's screen, in the browser (Rhyno, 22 Nov 2026: one screen, two places). The board
// sends the words on its screen, this page paints them with the board's own drawing, and a
// click here is a tap there: the board runs it like a finger on its panel.
const BoardScreen: FC = () => {
  const canvas = useRef<HTMLCanvasElement>(null);
  const [seen, setSeen] = useState(false);
  const [lost, setLost] = useState(false);

  useEffect(() => {
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    let misses = 0;
    let last = '';
    let picture: ImageData | undefined;  // made once, painted over on every new frame
    const poll = async () => {
      try {
        const model = (await WSoftApi.readWSoftScreen()).data;
        if (stopped) return;
        misses = 0;
        setLost(false);
        setSeen(true);
        const words = JSON.stringify(model);
        const context = canvas.current?.getContext('2d');
        if (context && words !== last) {
          if (!picture) picture = context.createImageData(PICTURE_W, PICTURE_H);
          toRgba(paintPicture(model), picture.data);
          context.putImageData(picture, 0, 0);
          last = words;
        }
      } catch {
        if (++misses >= LOST_AFTER) setLost(true);
      }
      // One request at a time: the next one starts after this one ends.
      if (!stopped) timer = setTimeout(poll, REFRESH_MS);
    };
    poll();
    return () => {
      stopped = true;
      clearTimeout(timer);
    };
  }, []);

  const onClick = (event: MouseEvent<HTMLCanvasElement>) => {
    const point = tapPoint(event.clientX, event.clientY, event.currentTarget.getBoundingClientRect());
    // The next picture shows what the tap did, or that it did nothing.
    if (point) WSoftApi.tapWSoftScreen(point).catch(() => undefined);
  };

  return (
    <>
      {lost && <MessageBox level="error" message="No answer from the board. This picture is old." my={2} />}
      {!seen && !lost && <FormLoader message="Reading the board's screen…" />}
      {/* Painted with the Waveshare's dots, so it stays sharp on a big screen (Rhyno, 22 Nov 2026). */}
      <Box
        sx={{
          maxWidth: 960,
          mx: 'auto',
          mb: 2,
          display: seen ? 'block' : 'none',
          opacity: lost ? 0.4 : 1,
          transition: 'opacity 0.3s',
          borderRadius: 3,
          overflow: 'hidden',
          boxShadow: '0 12px 32px rgba(27, 20, 70, 0.25)',
          lineHeight: 0
        }}
      >
        <canvas
          ref={canvas}
          width={PICTURE_W}
          height={PICTURE_H}
          onClick={onClick}
          role="img"
          aria-label="The board's screen. Click it like you tap the board."
          style={{ width: '100%', height: 'auto', display: 'block', cursor: 'pointer', touchAction: 'manipulation' }}
        />
      </Box>
    </>
  );
};

const alarmText = (status: WSoftStatus) =>
  (status.alarm_allowed ? 'Allowed, monitoring: ' + (status.alarm_note ?? '') : status.alarm ? 'ALARM (blocking)' : 'None') +
  (status.alarm_word_hex ? ` (code ${status.alarm_word_hex})` : '') +
  (status.alarm_changes > 0 ? `. CHANGED ${status.alarm_changes}x since start` : '');

// The board's state in words, under the screen (Rhyno, 23 Nov 2026: the screen AND the old info).
const StatusTable: FC<{ status: WSoftStatus }> = ({ status }) => {
  const rows: [string, string | number][] = [
    ['Trade check', status.trade_check],
    ['Audit counter', status.audit_counter],
    ['Last trade sequence', status.last_trade_seq || '-'],
    ['Waiting for the Hub', status.trade_queue],
    ['Hub', status.hub],
    ['Junction box alarm', alarmText(status)],
    ['Screen', status.display_ready ? 'Ready' : 'Not ready']
  ];
  return (
    <Table size="small">
      <TableBody>
        {rows.map(([name, value]) => (
          <TableRow key={name}>
            <TableCell><strong>{name}</strong></TableCell>
            <TableCell>{value}</TableCell>
          </TableRow>
        ))}
      </TableBody>
    </Table>
  );
};

const alarmTestText = (status: WSoftStatus) =>
  status.approval_on
    ? 'Alarm test switch is ON but does nothing while Approval is ON. Switch it off in Settings.'
    : 'TEST MODE: saves go through a junction box alarm. Those weighings are marked on the Hub. Switch it off in Settings.';

// Live: the board's screen on top, then the switches, Save and the board's state (every W-Soft board).
const WSoftLive: FC = () => {
  const { connected, data } = useWs<WSoftStatus>(WSOFT_STATUS_WEBSOCKET_URL);
  return (
    <SectionContent title="W-Soft live" titleGutter>
      <BoardScreen />
      {data?.alarm_test && <MessageBox level="warning" message={alarmTestText(data)} my={2} />}
      <WSoftSwitches />
      <WSoftSavePanel />
      {!connected || !data ? <FormLoader message="Connecting to WebSocket…" /> : <StatusTable status={data} />}
    </SectionContent>
  );
};

export default WSoftLive;
