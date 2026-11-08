import { FC, useEffect, useState } from 'react';
import { useSnackbar } from 'notistack';

import { Box, Button, MenuItem, Table, TableBody, TableCell, TableHead, TableRow, TextField, Typography } from '@mui/material';
import DownloadIcon from '@mui/icons-material/Download';

import * as WSoftApi from './api';
import { boardErrorText, plainError } from './errors';
import { WSoftCardField, WSoftDjbRequest, WSoftScaleCard } from './types';

interface CardFieldInfo {
  field: WSoftCardField;
  label: string;
  /** Fixed choices; free whole number when absent. */
  choices?: { value: number; label: string }[];
}

// The card settings the Scale setup needs, in the names /rest/djb uses (lib/wsoft_core/src/wsoft_djb.cpp).
// Decimal places and e are set in the form above; this table shows them read-only (READ_ONLY).
const FIELDS: CardFieldInfo[] = [
  { field: 'division', label: 'Step (e)' },
  {
    field: 'decimal',
    label: 'Decimal places',
    choices: [0, 1, 2, 3, 4].map((value) => ({ value, label: String(value) }))
  },
  {
    field: 'unit',
    label: 'Unit',
    choices: [
      { value: 0, label: 'No unit' },
      { value: 1, label: 'kg' },
      { value: 2, label: 't' },
      { value: 3, label: 'g' }
    ]
  },
  { field: 'zero_track_limit', label: 'Zero tracking range' },
  { field: 'zero_track_time', label: 'Zero tracking time' },
  { field: 'stability_range', label: 'Stable range' },
  { field: 'stability_time', label: 'Stable time' }
];

type Values = Partial<Record<WSoftCardField, number>>;

const READ_ONLY: WSoftCardField[] = ['division', 'decimal'];

const UNIT_TEXT = ['', 'kg', 't', 'g'];

/** e in the card's unit: the step divided by 10 to the decimal places. */
const stepText = (values: Values) => {
  if (values.division === undefined || values.decimal === undefined) return undefined;
  const e = (values.division / 10 ** values.decimal).toFixed(values.decimal);
  return `e = ${e} ${UNIT_TEXT[values.unit ?? 0] ?? ''}`.trim();
};

const shown = (info: CardFieldInfo, value?: number) => {
  if (value === undefined) return '-';
  return info.choices?.find((choice) => choice.value === value)?.label ?? String(value);
};

interface WSoftCardSetupProps {
  /** Last values the board read from the card (A4), or null. */
  card: WSoftScaleCard | null;
  /** Admin: may read through /rest/djb. */
  canRead: boolean;
  /** Admin and the setup is not locked. */
  canWrite: boolean;
  /** Called after a write, so the page reloads complete / missing and the audit count. */
  onWritten: () => void;
}

// Weighing card section of the Scale setup tab. Reads and writes go through the existing /rest/djb, one field each.
const WSoftCardSetup: FC<WSoftCardSetupProps> = ({ card, canRead, canWrite, onWritten }) => {
  const { enqueueSnackbar } = useSnackbar();
  const [values, setValues] = useState<Values>({});
  const [drafts, setDrafts] = useState<Partial<Record<WSoftCardField, string>>>({});
  const [busy, setBusy] = useState(false);

  useEffect(() => {
    if (card) setValues((old) => ({ ...old, division: card.division, decimal: card.decimal, unit: card.unit }));
  }, [card]);

  const call = async (request: WSoftDjbRequest) => {
    const noAnswer = 'The weighing card did not answer.';
    let problem: string;
    try {
      const reply = (await WSoftApi.djb(request)).data;
      if (reply.ok && reply.value !== undefined) {
        setValues((old) => ({ ...old, [request.field]: reply.value }));
        return true;
      }
      problem = plainError(reply.error) ?? noAnswer;
    } catch (error) {
      problem = boardErrorText(error, noAnswer);
    }
    const label = FIELDS.find((info) => info.field === request.field)?.label ?? request.field;
    enqueueSnackbar(`${label}: ${problem}`, { variant: 'error' });
    return false;
  };

  const readAll = async () => {
    setBusy(true);
    for (const info of FIELDS) {
      if (!(await call({ op: 'read', field: info.field }))) break;
    }
    setBusy(false);
  };

  const read = async (field: WSoftCardField) => {
    setBusy(true);
    await call({ op: 'read', field });
    setBusy(false);
  };

  const write = async (info: CardFieldInfo) => {
    const value = Number(drafts[info.field]);
    setBusy(true);
    if (await call({ op: 'write', field: info.field, value })) {
      enqueueSnackbar(`${info.label} saved on the weighing card`, { variant: 'success' });
      setDrafts((old) => ({ ...old, [info.field]: undefined }));
      await call({ op: 'read', field: info.field });
      onWritten();
    }
    setBusy(false);
  };

  const draftOk = (info: CardFieldInfo) => /^-?[0-9]+$/.test(drafts[info.field] ?? '');

  const step = stepText(values);

  return (
    <Box mt={3}>
      <Typography variant="h6">Weighing card</Typography>
      <Typography variant="body2" color="text.secondary">
        These live on the RS485 weighing card, not on this board.{step ? ` Now: ${step}.` : ''}
      </Typography>
      {canRead && (
        <Button startIcon={<DownloadIcon />} variant="outlined" sx={{ mt: 1 }} disabled={busy} onClick={readAll}>
          Read all from the card
        </Button>
      )}
      <Table size="small" sx={{ mt: 1 }}>
        <TableHead>
          <TableRow>
            <TableCell>Setting</TableCell>
            <TableCell>On the card</TableCell>
            {canWrite && <TableCell>New value</TableCell>}
            {canRead && <TableCell />}
          </TableRow>
        </TableHead>
        <TableBody>
          {FIELDS.map((info) => (
            <TableRow key={info.field}>
              <TableCell>{info.label}</TableCell>
              <TableCell>{shown(info, values[info.field])}</TableCell>
              {canWrite && READ_ONLY.includes(info.field) && <TableCell>Set above</TableCell>}
              {canWrite && !READ_ONLY.includes(info.field) && (
                <TableCell>
                  <TextField
                    size="small"
                    select={!!info.choices}
                    value={drafts[info.field] ?? ''}
                    onChange={(event) => setDrafts((old) => ({ ...old, [info.field]: event.target.value }))}
                    inputProps={info.choices ? undefined : { inputMode: 'numeric' }}
                    sx={{ minWidth: 120 }}
                    aria-label={`New ${info.label}`}
                  >
                    {info.choices?.map((choice) => (
                      <MenuItem key={choice.value} value={String(choice.value)}>{choice.label}</MenuItem>
                    ))}
                  </TextField>
                </TableCell>
              )}
              {canRead && (
                <TableCell sx={{ whiteSpace: 'nowrap' }}>
                  <Button size="small" disabled={busy} onClick={() => read(info.field)}>Read</Button>
                  {canWrite && !READ_ONLY.includes(info.field) && (
                    <Button size="small" disabled={busy || !draftOk(info)} onClick={() => write(info)}>Write</Button>
                  )}
                </TableCell>
              )}
            </TableRow>
          ))}
        </TableBody>
      </Table>
    </Box>
  );
};

export default WSoftCardSetup;
