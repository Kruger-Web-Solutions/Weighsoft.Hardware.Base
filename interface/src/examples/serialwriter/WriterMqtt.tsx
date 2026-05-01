import React, { FC } from 'react';
import { Card, CardContent, TextField, Switch, FormControlLabel, Button, Alert } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';
import { SectionContent, FormLoader, ButtonRow } from '../../components';
import { useRest } from '../../utils';
import { readSerialWriter, updateSerialWriter } from '../../api/serialWriter';
import { SerialWriterData } from '../../types/serialWriter';

const WriterMqtt: FC = () => {
  const {
    data,
    loadData,
    saveData,
    saving,
    setData,
    errorMessage,
  } = useRest<SerialWriterData>({ read: readSerialWriter, update: updateSerialWriter });

  if (!data) {
    return (
      <SectionContent title="MQTT" titleGutter>
        <FormLoader onRetry={loadData} errorMessage={errorMessage} />
      </SectionContent>
    );
  }

  const setField = <K extends keyof SerialWriterData>(k: K) => (v: SerialWriterData[K]) =>
    setData((p) => p ? { ...p, [k]: v } : p);

  return (
    <SectionContent title="MQTT" titleGutter>
      <Alert severity="info" sx={{ mb: 2 }}>
        Subscribe to a topic and any message it receives gets written to the serial port.
      </Alert>

      <Card>
        <CardContent>
          <FormControlLabel
            control={
              <Switch
                checked={data.mqtt_enabled}
                onChange={(e) => setField('mqtt_enabled')(e.target.checked)}
              />
            }
            label="Enabled"
            sx={{ mb: 2, display: 'block' }}
          />

          <TextField
            fullWidth
            label="Subscribe topic"
            value={data.mqtt_subscribe_topic}
            onChange={(e) => setField('mqtt_subscribe_topic')(e.target.value)}
            helperText={`Suggested: weighsoft/serialwriter/${data.device_id}/send`}
            sx={{ mb: 2 }}
          />

          <TextField
            fullWidth
            label="Status publish topic"
            value={data.mqtt_status_topic}
            onChange={(e) => setField('mqtt_status_topic')(e.target.value)}
            helperText={`Suggested: weighsoft/serialwriter/${data.device_id}/status`}
            sx={{ mb: 2 }}
          />

          <ButtonRow>
            <Button
              startIcon={<SaveIcon />}
              variant="contained"
              disabled={saving}
              onClick={saveData}
            >
              Save MQTT
            </Button>
          </ButtonRow>
        </CardContent>
      </Card>
    </SectionContent>
  );
};

export default WriterMqtt;
