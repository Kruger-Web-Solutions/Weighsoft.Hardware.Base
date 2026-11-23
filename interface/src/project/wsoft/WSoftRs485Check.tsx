import { FC } from 'react';

import { Box, Button, Table, TableBody, TableCell, TableHead, TableRow, Typography } from '@mui/material';
import DownloadIcon from '@mui/icons-material/Download';

import { CardValues, CHECK_ROWS, CHECK_TEXT, Rs485Form, rowCheck, typedValue, valueText } from './rs485Setup';
import { WSoftCardField, WSoftScaleSetup } from './types';

interface WSoftRs485CheckProps {
  setup: WSoftScaleSetup;
  /** What is typed in the form above. */
  form: Rs485Form;
  /** What the card answered on the last read, in its own numbers. */
  card: CardValues;
  /** The board's own copy (W1X format and the tuning), in card numbers. */
  board: CardValues;
  /** Admin: may read the card. */
  canRead: boolean;
  busy: boolean;
  onRead: (fields: WSoftCardField[]) => void;
}

// Under the RS485 Scale setup: one row per value typed above, what the card holds, and where it is
// saved. Read only: every value is set in the form above and written by its Save.
const WSoftRs485Check: FC<WSoftRs485CheckProps> = ({ setup, form, card, board, canRead, busy, onRead }) => {
  const formatOnBoard = !!setup.format_on_board;
  // e is shown in kg at the decimal places the card itself reported, when it reported them.
  const cardDecimal = card.decimal !== undefined && card.decimal >= 0 && card.decimal <= 4 ? card.decimal : undefined;
  const typedDecimal = typedValue(form, 'decimal');

  const zero = setup.zero;
  return (
    <Box mt={3}>
      <Typography variant="h6">Check: what the weighing card holds</Typography>
      <Typography variant="body2" color="text.secondary">
        {formatOnBoard
          ? 'This card is a DJB-W1X (no display). It keeps only Filter 1 and Filter 2. Unit, decimal places, e, stable '
            + 'and auto zero have no setting on it, so this board keeps them and does that work itself.'
          : 'Every value typed above is written to the weighing card when you press Save, then read back here.'}
      </Typography>
      {canRead && (
        <Button
          startIcon={<DownloadIcon />}
          variant="outlined"
          sx={{ mt: 1 }}
          disabled={busy}
          onClick={() => onRead(CHECK_ROWS.map((row) => row.field))}
        >
          Read all from the card
        </Button>
      )}
      <Table size="small" sx={{ mt: 1 }}>
        <TableHead>
          <TableRow>
            <TableCell>Setting</TableCell>
            <TableCell>Typed above</TableCell>
            <TableCell>On the card</TableCell>
            <TableCell>Check</TableCell>
            {canRead && <TableCell />}
          </TableRow>
        </TableHead>
        <TableBody>
          {CHECK_ROWS.map((row) => {
            const state = rowCheck(row.field, form, formatOnBoard, card, board);
            const good = state === 'same' || state === 'board';
            return (
              <TableRow key={row.field}>
                <TableCell>{row.label}</TableCell>
                <TableCell>{valueText(row.field, typedValue(form, row.field), typedDecimal)}</TableCell>
                <TableCell>{valueText(row.field, card[row.field], cardDecimal)}</TableCell>
                <TableCell sx={{ color: good ? 'success.main' : state === 'unread' ? 'text.secondary' : 'error.main' }}>
                  {CHECK_TEXT[state]}
                </TableCell>
                {canRead && (
                  <TableCell>
                    <Button size="small" disabled={busy} onClick={() => onRead([row.field])}>Read</Button>
                  </TableCell>
                )}
              </TableRow>
            );
          })}
        </TableBody>
      </Table>
      {zero && (
        <Typography variant="body2" color="text.secondary" mt={2}>
          ZERO (the button on the screen): {zero.set ? `set, the card's ${zero.raw} reads 0` : 'not set'}.
          {setup.zero_on_board ? ` Auto zero on this board has taken off ${zero.auto}.` : ''}
          {' '}Nothing is written to the card by ZERO: its calibration stays as it is.
        </Typography>
      )}
    </Box>
  );
};

export default WSoftRs485Check;
