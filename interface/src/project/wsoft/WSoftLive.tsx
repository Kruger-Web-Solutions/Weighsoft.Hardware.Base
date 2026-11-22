import { FC, MouseEvent, useEffect, useRef, useState } from 'react';

import { Box } from '@mui/material';

import { FormLoader, MessageBox, SectionContent } from '../../components';

import * as WSoftApi from './api';
import { paintPicture, tapPoint, toRgba, PICTURE_H, PICTURE_W } from './screen';

const REFRESH_MS = 250;
const LOST_AFTER = 8;  // misses in a row (about 2 s) before the page says the board is gone

// The board's screen, in the browser (Rhyno, 22 Nov 2026: one screen, two places). The board
// sends the words on its screen, this page paints them with the board's own drawing, and a
// click here is a tap there: the board runs it like a finger on its panel. So this page shows
// what the board shows and can do what the board's screen can do, and nothing else.
const WSoftLive: FC = () => {
  const canvas = useRef<HTMLCanvasElement>(null);
  const [seen, setSeen] = useState(false);
  const [lost, setLost] = useState(false);

  useEffect(() => {
    let stopped = false;
    let timer: ReturnType<typeof setTimeout>;
    let misses = 0;
    let last = '';
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
          const picture = context.createImageData(PICTURE_W, PICTURE_H);
          picture.data.set(toRgba(paintPicture(model)));
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
    <SectionContent title="W-Soft screen" titleGutter>
      {lost && <MessageBox level="error" message="No answer from the board. This picture is old." my={2} />}
      {!seen && !lost && <FormLoader message="Reading the board's screen…" />}
      {/* Painted with the Waveshare's dots, so it stays sharp on a big screen (Rhyno, 22 Nov 2026). */}
      <Box
        sx={{
          maxWidth: 960,
          mx: 'auto',
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
    </SectionContent>
  );
};

export default WSoftLive;
