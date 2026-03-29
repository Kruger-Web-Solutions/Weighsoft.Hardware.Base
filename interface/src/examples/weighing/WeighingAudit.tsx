import React, { FC, useState, useEffect, useCallback } from 'react';
import {
  Typography,
  Box,
  Button,
  Table,
  TableBody,
  TableCell,
  TableHead,
  TableRow,
  Alert,
} from '@mui/material';
import RefreshIcon from '@mui/icons-material/Refresh';
import { SectionContent } from '../../components';
import { readWeighingAudit } from '../../api/weighing';
import { AuditEntry } from '../../types/weighing';

const WeighingAudit: FC = () => {
  const [entries, setEntries] = useState<AuditEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState('');

  const fetchAudit = useCallback(async () => {
    setLoading(true);
    setError('');
    try {
      const res = await readWeighingAudit();
      setEntries(Array.isArray(res.data) ? res.data : []);
    } catch (e: any) {
      setError(e?.message || 'Failed to load audit trail');
    }
    setLoading(false);
  }, []);

  useEffect(() => {
    fetchAudit();
  }, [fetchAudit]);

  return (
    <SectionContent title="Audit Trail (OIML 6.2.3.6)" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Read-only log of all legally relevant events. Admin access required. Cannot be deleted
        or cleared via the API. Survives OTA firmware updates.
      </Alert>

      <Box sx={{ display: 'flex', justifyContent: 'flex-end', mb: 2 }}>
        <Button
          startIcon={<RefreshIcon />}
          onClick={fetchAudit}
          disabled={loading}
          variant="outlined"
          size="small"
        >
          {loading ? 'Loading...' : 'Refresh'}
        </Button>
      </Box>

      {error && <Alert severity="error" sx={{ mb: 2 }}>{error}</Alert>}

      {entries.length === 0 && !loading && !error && (
        <Typography color="text.secondary">No audit entries yet.</Typography>
      )}

      {entries.length > 0 && (
        <Box sx={{ overflowX: 'auto' }}>
          <Table size="small">
            <TableHead>
              <TableRow>
                <TableCell>#</TableCell>
                <TableCell>Timestamp</TableCell>
                <TableCell>Event</TableCell>
                <TableCell>Parameter</TableCell>
                <TableCell>Old Value</TableCell>
                <TableCell>New Value</TableCell>
                <TableCell>Event Counter</TableCell>
              </TableRow>
            </TableHead>
            <TableBody>
              {entries.map((entry) => (
                <TableRow key={entry.seq} hover>
                  <TableCell>{entry.seq}</TableCell>
                  <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.75rem', whiteSpace: 'nowrap' }}>
                    {entry.timestamp}
                  </TableCell>
                  <TableCell>
                    <Typography
                      variant="body2"
                      sx={{
                        fontWeight: 'bold',
                        color:
                          entry.event === 'seal' ? 'error.main' :
                          entry.event === 'unseal' ? 'success.main' :
                          entry.event === 'unseal_fail' ? 'error.main' :
                          'text.primary',
                      }}
                    >
                      {entry.event}
                    </Typography>
                  </TableCell>
                  <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>{entry.parameter}</TableCell>
                  <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>{entry.old_value}</TableCell>
                  <TableCell sx={{ fontFamily: 'monospace', fontSize: '0.8rem' }}>{entry.new_value}</TableCell>
                  <TableCell align="center">{entry.event_counter}</TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </Box>
      )}
    </SectionContent>
  );
};

export default WeighingAudit;
