import { FC, useState } from 'react';
import { ValidateFieldsError } from 'async-validator';

import { Button, Checkbox, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import {
  BlockFormControlLabel, ButtonRow, FormLoader, MessageBox, SectionContent, ValidatedTextField
} from '../../components';
import { validate } from '../../validators';
import { numberValue, updateValue, useRest } from '../../utils';

import * as WSoftApi from './api';
import { WSoftSettings } from './types';
import { WSOFT_SETTINGS_VALIDATOR } from './validators';

const WSoftSettingsForm: FC = () => {
  const [fieldErrors, setFieldErrors] = useState<ValidateFieldsError>();
  const {
    loadData, saving, data, setData, saveData, errorMessage
  } = useRest<WSoftSettings>({ read: WSoftApi.readWSoftSettings, update: WSoftApi.updateWSoftSettings });

  const updateFormValue = updateValue(setData);

  const content = () => {
    if (!data) {
      return (<FormLoader onRetry={loadData} errorMessage={errorMessage} />);
    }

    const validateAndSubmit = async () => {
      try {
        setFieldErrors(undefined);
        await validate(WSOFT_SETTINGS_VALIDATOR, data);
        saveData();
      } catch (errors: any) {
        setFieldErrors(errors);
      }
    };

    return (
      <>
        <MessageBox
          level="info"
          message={
            'Wi-Fi, MQTT broker, NTP and OTA are on the framework pages. ' +
            'Hub pairing is on the Hub tab. Approval is set with a USB cable only.'
          }
          my={2}
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="publish_interval_ms"
          label="Publish interval (ms)"
          fullWidth
          variant="outlined"
          value={numberValue(data.publish_interval_ms)}
          type="number"
          onChange={updateFormValue}
          margin="normal"
          helperText="How often the live page and MQTT may update (250 to 60000 ms)"
        />
        <BlockFormControlLabel
          control={
            <Checkbox
              name="mqtt_enabled"
              checked={data.mqtt_enabled}
              onChange={updateFormValue}
            />
          }
          label="Publish W-Soft status over MQTT?"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="mqtt_base_path"
          label="MQTT base path"
          fullWidth
          variant="outlined"
          value={data.mqtt_base_path}
          onChange={updateFormValue}
          margin="normal"
          helperText="Supports #{unique_id} and #{platform}"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="mqtt_name"
          label="MQTT name"
          fullWidth
          variant="outlined"
          value={data.mqtt_name}
          onChange={updateFormValue}
          margin="normal"
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="mqtt_unique_id"
          label="MQTT unique id"
          fullWidth
          variant="outlined"
          value={data.mqtt_unique_id}
          onChange={updateFormValue}
          margin="normal"
        />
        {data.mqtt_status_topic && (
          <Typography variant="body2" color="text.secondary" sx={{ mt: 1 }}>
            Status topic: {data.mqtt_status_topic}
          </Typography>
        )}
        <ButtonRow mt={1}>
          <Button
            startIcon={<SaveIcon />}
            disabled={saving}
            variant="contained"
            color="primary"
            type="submit"
            onClick={validateAndSubmit}
          >
            Save
          </Button>
        </ButtonRow>
      </>
    );
  };

  return (
    <SectionContent title="W-Soft settings" titleGutter>
      {content()}
    </SectionContent>
  );
};

export default WSoftSettingsForm;
