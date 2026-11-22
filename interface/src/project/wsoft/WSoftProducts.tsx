import { FC, useCallback, useContext, useEffect, useState } from 'react';
import { useSnackbar } from 'notistack';

import {
  Box, Button, Chip, Dialog, DialogActions, DialogContent, DialogTitle, IconButton, Table, TableBody, TableCell,
  TableHead, TableRow, TextField, Tooltip, Typography
} from '@mui/material';
import CheckIcon from '@mui/icons-material/Check';
import DeleteIcon from '@mui/icons-material/Delete';
import EditIcon from '@mui/icons-material/Edit';
import LockIcon from '@mui/icons-material/Lock';
import SaveIcon from '@mui/icons-material/Save';

import { ButtonRow, MessageBox, SectionContent } from '../../components';
import { AuthenticationContext } from '../../contexts/authentication';

import * as WSoftApi from './api';
import { boardErrorText } from './errors';
import { checkForm, EMPTY_FORM, formToProduct, gramsToKgShort, ProductForm, productToForm } from './products';
import { WSoftProduct, WSoftProductAction, WSoftProducts as Products } from './types';

const limits = (p: WSoftProduct) =>
  `${gramsToKgShort(p.low_g)} / ${gramsToKgShort(p.target_g)} / ${gramsToKgShort(p.high_g)} kg`;

// W-Soft > Products: Hub products (read-only) and products made on this board.
const WSoftProducts: FC = () => {
  const { enqueueSnackbar } = useSnackbar();
  const { me } = useContext(AuthenticationContext);
  const isAdmin = !!me?.admin;
  const [data, setData] = useState<Products>();
  const [loadError, setLoadError] = useState<string>();
  const [form, setForm] = useState<ProductForm>(EMPTY_FORM);
  const [editing, setEditing] = useState('');
  const [touched, setTouched] = useState(false);
  const [busy, setBusy] = useState(false);
  const [deleting, setDeleting] = useState<WSoftProduct>();

  const load = useCallback(async () => {
    try {
      setData((await WSoftApi.readWSoftProducts()).data);
      setLoadError(undefined);
    } catch (error) {
      setLoadError(boardErrorText(error, 'Could not read the products'));
    }
  }, []);

  useEffect(() => {
    load();
  }, [load]);

  const send = async (action: WSoftProductAction, done: string) => {
    setBusy(true);
    try {
      setData((await WSoftApi.updateWSoftProducts(action)).data);
      enqueueSnackbar(done, { variant: 'success' });
      return true;
    } catch (error) {
      enqueueSnackbar(boardErrorText(error, 'The board did not take the change'), { variant: 'error' });
      return false;
    } finally {
      setBusy(false);
    }
  };

  const hubCodes = (data?.products ?? []).filter((p) => p.from === 'hub').map((p) => p.code);
  const boardCount = (data?.products ?? []).filter((p) => p.from === 'board').length;
  const problems = checkForm(form, hubCodes);
  const ready = Object.keys(problems).length === 0;
  const show = (field: keyof ProductForm) => (touched || form[field] !== '' ? problems[field] : undefined);

  const clearForm = () => {
    setForm(EMPTY_FORM);
    setEditing('');
    setTouched(false);
  };

  const save = async () => {
    setTouched(true);
    if (!ready) return;
    const ok = await send(
      { action: 'put', old_code: editing, product: formToProduct(form) },
      editing ? `${form.code} saved` : `${form.code} added`
    );
    if (ok) clearForm();
  };

  const remove = async () => {
    if (!deleting) return;
    const code = deleting.code;
    setDeleting(undefined);
    await send({ action: 'delete', code }, `${code} deleted`);
    if (editing === code) clearForm();
  };

  if (loadError) {
    return (
      <SectionContent title="Products" titleGutter>
        <MessageBox level="error" message={loadError} my={1} />
        <Button variant="outlined" onClick={load}>Try again</Button>
      </SectionContent>
    );
  }

  const field = (key: keyof ProductForm, label: string, extra?: object) => (
    <TextField
      label={label}
      value={form[key]}
      onChange={(event) => setForm({ ...form, [key]: event.target.value })}
      error={!!show(key)}
      helperText={show(key) ?? ' '}
      InputLabelProps={{ shrink: true }}
      fullWidth
      margin="dense"
      {...extra}
    />
  );

  return (
    <>
      <SectionContent title="Products on this board" titleGutter>
        <Typography variant="body2" color="text.secondary" mb={1}>
          Hub products come from the Hub and can only be changed there. Board products are made here and stay on this
          board. Every weighing carries its product code, so the Hub still sees which product was weighed.
        </Typography>
        {!data ? (
          <Typography>Loading…</Typography>
        ) : data.products.length === 0 ? (
          <MessageBox level="info" message="No products yet. Add one below, or set limits on the Hub." my={1} />
        ) : (
          <Box sx={{ overflowX: 'auto' }}>
            <Table size="small">
              <TableHead>
                <TableRow>
                  <TableCell>Code</TableCell>
                  <TableCell>Name</TableCell>
                  <TableCell>Low / Target / High</TableCell>
                  <TableCell>From</TableCell>
                  <TableCell />
                </TableRow>
              </TableHead>
              <TableBody>
                {data.products.map((p) => {
                  const inUse = !p.hidden && p.code === data.current;
                  return (
                    <TableRow key={`${p.from}-${p.code}`} sx={{ opacity: p.hidden ? 0.5 : 1 }}>
                      <TableCell sx={{ fontWeight: 'bold', whiteSpace: 'nowrap' }}>{p.code}</TableCell>
                      <TableCell>{p.name}</TableCell>
                      <TableCell sx={{ whiteSpace: 'nowrap' }}>{limits(p)}</TableCell>
                      <TableCell>
                        <Chip
                          size="small"
                          label={p.from === 'hub' ? 'Hub' : p.hidden ? 'Hidden (Hub code)' : 'This board'}
                          color={p.from === 'hub' ? 'primary' : p.hidden ? 'default' : 'success'}
                          variant="outlined"
                        />
                      </TableCell>
                      <TableCell sx={{ whiteSpace: 'nowrap' }}>
                        {inUse ? (
                          <Chip size="small" color="primary" icon={<CheckIcon />} label="In use" />
                        ) : (
                          isAdmin &&
                          !p.hidden && (
                            <Button
                              size="small"
                              variant="outlined"
                              disabled={busy}
                              onClick={() => send({ action: 'use', code: p.code }, `Now using ${p.code}`)}
                            >
                              Use
                            </Button>
                          )
                        )}
                        {p.from === 'hub' ? (
                          <Tooltip title="Change Hub products on the Hub">
                            <LockIcon fontSize="small" sx={{ ml: 1, verticalAlign: 'middle', color: 'text.disabled' }} />
                          </Tooltip>
                        ) : (
                          isAdmin && (
                            <>
                              <IconButton
                                aria-label={`Edit ${p.code}`}
                                size="small"
                                disabled={busy}
                                onClick={() => {
                                  setForm(productToForm(p));
                                  setEditing(p.code);
                                  setTouched(false);
                                }}
                              >
                                <EditIcon fontSize="small" />
                              </IconButton>
                              <IconButton
                                aria-label={`Delete ${p.code}`}
                                size="small"
                                disabled={busy}
                                onClick={() => setDeleting(p)}
                              >
                                <DeleteIcon fontSize="small" />
                              </IconButton>
                            </>
                          )
                        )}
                      </TableCell>
                    </TableRow>
                  );
                })}
              </TableBody>
            </Table>
          </Box>
        )}
        {isAdmin && data && data.current !== '' && (
          <Box mt={1}>
            <Button size="small" disabled={busy} onClick={() => send({ action: 'use', code: '' }, 'No product in use')}>
              Use no product
            </Button>
          </Box>
        )}
      </SectionContent>

      {isAdmin && (
        <SectionContent title={editing ? `Edit ${editing}` : 'Add a product'} titleGutter>
          <Typography variant="body2" color="text.secondary">
            Saved on this board. Works with no Hub. {boardCount} of {data?.board_max ?? 20} board products used.
          </Typography>
          {field('code', 'Code (short, no spaces)', { inputProps: { maxLength: 32 } })}
          {field('name', 'Name', { inputProps: { maxLength: 24 } })}
          <Box display="flex" gap={1} flexWrap="wrap">
            <Box flex="1 1 100px">{field('low', 'Low (kg)', { inputProps: { inputMode: 'decimal' } })}</Box>
            <Box flex="1 1 100px">{field('target', 'Target (kg)', { inputProps: { inputMode: 'decimal' } })}</Box>
            <Box flex="1 1 100px">{field('high', 'High (kg)', { inputProps: { inputMode: 'decimal' } })}</Box>
          </Box>
          <ButtonRow mt={1}>
            <Button startIcon={<SaveIcon />} variant="contained" color="primary" disabled={busy} onClick={save}>
              Save product
            </Button>
            {(editing || form !== EMPTY_FORM) && (
              <Button disabled={busy} onClick={clearForm}>
                Cancel
              </Button>
            )}
          </ButtonRow>
        </SectionContent>
      )}

      <Dialog open={!!deleting} onClose={() => setDeleting(undefined)}>
        <DialogTitle>Delete {deleting?.code}?</DialogTitle>
        <DialogContent>
          <Typography>
            {deleting?.code === data?.current
              ? 'It is in use now. After deleting, no product is in use until you pick one.'
              : 'It is removed from this board. Weighings already saved keep their product code.'}
          </Typography>
        </DialogContent>
        <DialogActions>
          <Button onClick={() => setDeleting(undefined)}>Cancel</Button>
          <Button color="error" variant="contained" onClick={remove}>
            Delete
          </Button>
        </DialogActions>
      </Dialog>
    </>
  );
};

export default WSoftProducts;
