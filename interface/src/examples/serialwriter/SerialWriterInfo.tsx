import { FC } from 'react';
import { Alert, Box, Typography } from '@mui/material';
import OutputIcon from '@mui/icons-material/Output';
import { SectionContent } from '../../components';
import { UartModeSwitcher } from '../../components';

const SerialWriterInfo: FC = () => {
  return (
    <Box>
      <UartModeSwitcher currentMode="writer" />
      <SectionContent title="Serial Writer" titleGutter>
        <Alert severity="info" icon={<OutputIcon />} sx={{ mb: 3 }}>
          Serial Writer sends data <strong>to</strong> the Serial1 TX pin (GPIO17). This is the reverse of the
          Serial Monitor, which reads from RX (GPIO18). Both share the same UART hardware — switch UART Mode to{' '}
          <strong>Serial Writer</strong> before sending data.
        </Alert>

        <Typography variant="h6" gutterBottom>
          Hardware
        </Typography>
        <Box component="table" sx={{ width: '100%', borderCollapse: 'collapse', mb: 3 }}>
          <Box component="tbody">
            {[
              ['TX pin', 'GPIO17 (Serial1 TXD)'],
              ['RX pin', 'GPIO18 (Serial1 RXD, not used in writer mode)'],
              ['Default baud rate', '9600'],
              ['Line terminator', 'LF (configurable)'],
            ].map(([label, value]) => (
              <Box
                component="tr"
                key={label}
                sx={{ borderBottom: 1, borderColor: 'divider' }}
              >
                <Box component="td" sx={{ p: 1, color: 'text.secondary', width: '40%' }}>
                  {label}
                </Box>
                <Box component="td" sx={{ p: 1, fontFamily: 'monospace' }}>
                  {value}
                </Box>
              </Box>
            ))}
          </Box>
        </Box>

        <Typography variant="h6" gutterBottom>
          REST API
        </Typography>
        <Typography variant="body2" gutterBottom>
          <strong>GET /rest/serialWriter</strong> — read current state and config.
        </Typography>
        <Typography variant="body2" gutterBottom>
          <strong>POST /rest/serialWriter</strong> — send a line or update config. To send a line:
        </Typography>
        <Box
          component="pre"
          sx={{ bgcolor: 'background.paper', p: 2, borderRadius: 1, fontSize: '0.8rem', overflowX: 'auto', mb: 2 }}
        >
          {`POST /rest/serialWriter\n{"pending_line": "your text here"}`}
        </Box>
        <Typography variant="body2">
          The firmware appends the configured line terminator (LF/CRLF/CR/NONE) automatically.
        </Typography>

        <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
          WebSocket
        </Typography>
        <Typography variant="body2">
          Subscribe to <strong>/ws/serialWriter</strong> for live status updates including{' '}
          <code>last_sent_line</code> and <code>sent_count</code>. You can also send JSON with{' '}
          <code>pending_line</code> through the WebSocket to trigger a write.
        </Typography>

        <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
          MQTT
        </Typography>
        <Typography variant="body2">
          Publish to <strong>weighsoft/serialwriter/&lt;id&gt;/send</strong> with JSON payload{' '}
          <code>{`{"pending_line":"your text"}`}</code>. Status is published on{' '}
          <strong>weighsoft/serialwriter/&lt;id&gt;/status</strong>.
        </Typography>
      </SectionContent>
    </Box>
  );
};

export default SerialWriterInfo;
