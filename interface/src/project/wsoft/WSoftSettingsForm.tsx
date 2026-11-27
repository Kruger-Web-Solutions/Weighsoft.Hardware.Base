import { ChangeEvent, FC, useState } from 'react';
import { ValidateFieldsError } from 'async-validator';

import { Button, Checkbox, MenuItem, TextField, Typography } from '@mui/material';
import SaveIcon from '@mui/icons-material/Save';

import {
  BlockFormControlLabel, ButtonRow, FormLoader, MessageBox, SectionContent, ValidatedTextField
} from '../../components';
import { validate } from '../../validators';
import { numberValue, updateValue, useRest } from '../../utils';

import WSoftPinForm from './WSoftPinForm';
import WSoftRelayTest from './WSoftRelayTest';
import WSoftWifiUpdateForm from './WSoftWifiUpdateForm';
import * as WSoftApi from './api';
import { DEFAULT_PACK_SET, formatPackTargets, PACK_SET_MAX, parsePackTargets, toleranceProblem } from './packSet';
import { WSoftPackSet, WSoftSettings } from './types';
import { WSOFT_SETTINGS_VALIDATOR } from './validators';

const RELAY_ZONES: { name: 'relay_low' | 'relay_ok' | 'relay_high'; label: string }[] = [
  { name: 'relay_low', label: 'Relay for TOO LOW' },
  { name: 'relay_ok', label: 'Relay for OK' },
  { name: 'relay_high', label: 'Relay for TOO HIGH' }
];

// Every W-Soft board shows the same options (Rhyno, 23 Nov 2026: the yellow board is the
// reference). One this board is not wired for stays visible but greyed, with this note, and
// the board still refuses it, so nothing drives a pin that is not there.
const NEEDS_WIRING = 'Needs add-on wiring on this board.';

const WSoftSettingsForm: FC = () => {
  const [fieldErrors, setFieldErrors] = useState<ValidateFieldsError>();
  const [targetsInput, setTargetsInput] = useState<string>();
  const [targetsError, setTargetsError] = useState<string>();
  const [toleranceError, setToleranceError] = useState<string>();
  const {
    loadData, saving, data, setData, saveData, errorMessage
  } = useRest<WSoftSettings>({ read: WSoftApi.readWSoftSettings, update: WSoftApi.updateWSoftSettings });

  const updateFormValue = updateValue(setData);

  const content = () => {
    if (!data) {
      return (<FormLoader onRetry={loadData} errorMessage={errorMessage} />);
    }

    const packSet: WSoftPackSet = data.pack_set ?? DEFAULT_PACK_SET;
    const packMax = data.pack_set_max ?? PACK_SET_MAX;
    const shownTargets = targetsInput !== undefined ? targetsInput : formatPackTargets(packSet.targets_g);

    const updatePackSet = (patch: Partial<WSoftPackSet>) => {
      setData({ ...data, pack_set: { ...packSet, ...patch } });
    };

    const handleTargetsChange = (event: ChangeEvent<HTMLInputElement>) => {
      const nextText = event.target.value;
      setTargetsInput(nextText);
      const parsed = parsePackTargets(nextText, packMax);
      setTargetsError(parsed.error);
      if (!parsed.error) {
        updatePackSet({ targets_g: parsed.targets });
      }
    };

    const handleToleranceChange = (event: ChangeEvent<HTMLInputElement>) => {
      const raw = event.target.value;
      const num = raw === '' ? NaN : Number(raw);
      const err = toleranceProblem(num);
      setToleranceError(err);
      updatePackSet({ tolerance_g: Number.isNaN(num) ? -1 : num });
    };

    const validateAndSubmit = async () => {
      const parsedTargets = parsePackTargets(shownTargets, packMax);
      const tolErr = toleranceProblem(packSet.tolerance_g);
      setTargetsError(parsedTargets.error);
      setToleranceError(tolErr);
      try {
        setFieldErrors(undefined);
        await validate(WSOFT_SETTINGS_VALIDATOR, data);
        if (parsedTargets.error || tolErr) return;
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
            'Wi-Fi, MQTT broker and NTP are on the framework pages. Wi-Fi update is at the bottom of this page. ' +
            'Hub pairing is on the Hub item in the menu. Approval and Auto save are switched on the Live tab.'
          }
          my={2}
        />
        <ValidatedTextField
          fieldErrors={fieldErrors}
          name="source"
          label="Weight comes from"
          value={data.source}
          fullWidth
          select
          variant="outlined"
          onChange={updateFormValue}
          margin="normal"
          helperText={
            'RS232: set the speed and the weight pattern on the Serial page. ' +
            'Load cell: calibrate it on the Calibrate tab. ' +
            'All of them use the same plug, so the board has one at a time. Saving a new choice restarts the board.'
          }
        >
          <MenuItem value="rs485">Junction box (RS485)</MenuItem>
          <MenuItem value="rs232" disabled={!data.rs232_available}>
            {data.rs232_available ? 'RS232 scale' : 'RS232 scale (needs add-on wiring)'}
          </MenuItem>
          <MenuItem value="loadcell" disabled={!data.loadcell_available}>
            {data.loadcell_available ? 'Load cell (HX711 board)' : 'Load cell (needs add-on wiring)'}
          </MenuItem>
        </ValidatedTextField>
        <>
          {!data.relay_available && (
            <Typography variant="body2" color="text.secondary" sx={{ mt: 2 }}>
              Hi-Lo relays: {NEEDS_WIRING}
            </Typography>
          )}
          <BlockFormControlLabel
            disabled={!data.relay_available}
            control={
              <Checkbox
                name="relay_enabled"
                checked={data.relay_enabled}
                onChange={updateFormValue}
              />
            }
            label="Switch the Hi-Lo relays (LOW / OK / HIGH)?"
          />
          {RELAY_ZONES.map((zone) => (
            <ValidatedTextField
              key={zone.name}
              fieldErrors={fieldErrors}
              name={zone.name}
              label={zone.label}
              value={data[zone.name]}
              fullWidth
              select
              variant="outlined"
              disabled={!data.relay_available}
              onChange={(event) => setData({ ...data, [zone.name]: Number(event.target.value) })}
              margin="normal"
            >
              <MenuItem value={0}>None</MenuItem>
              <MenuItem value={1}>Relay 1</MenuItem>
              <MenuItem value={2}>Relay 2</MenuItem>
              <MenuItem value={3}>Relay 3</MenuItem>
              <MenuItem value={4}>Relay 4</MenuItem>
            </ValidatedTextField>
          ))}
          <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
            All relays are off with no signal, an alarm, no product or an empty scale.
          </Typography>
          <BlockFormControlLabel
            disabled={!data.relay_available}
            control={
              <Checkbox
                name="relay_writer_follow"
                checked={!!data.relay_writer_follow}
                onChange={updateFormValue}
              />
            }
            label="In Writer mode, switch the relays too (they follow the weigh board's weight)?"
          />
        </>
        <>
          <BlockFormControlLabel
            disabled={!data.relay_light_available}
            control={<Checkbox name="relay_light" checked={!!data.relay_light} onChange={updateFormValue} />}
            label="Hi-Lo light: show Hi-Lo on the board's light (blue under, green correct, red over)?"
          />
          <Typography variant="body2" color="text.secondary" sx={{ mb: 1 }}>
            {data.relay_light_available
              ? 'The light uses the relay pins. A relay board wired to the light switches with it. Untick to keep the light dark.'
              : NEEDS_WIRING}
          </Typography>
        </>
        {data.esp_led_available && (
          <BlockFormControlLabel
            control={<Checkbox name="esp_led" checked={!!data.esp_led} onChange={updateFormValue} />}
            label="ESP LED: show Hi-Lo on the board's light (orange under, green correct, red over)?"
          />
        )}
        <BlockFormControlLabel
          control={
            <Checkbox name="alarm_test_save"checked={!!data.alarm_test_save} onChange={updateFormValue} />
          }
          label="TEST ONLY: save even when the junction box shows an alarm?"
        />
        {data.alarm_test_save && (
          <MessageBox
            level="warning"
            message={
              'Test switch is ON. While Approval is OFF, an alarm (load cell fault, over range) no longer stops a save. ' +
              'Every weighing saved like that is marked "alarm_test_override" on the Hub and is never for trade. ' +
              'With Approval ON this switch does nothing. Switch it off when the test is done.'
            }
            my={1}
          />
        )}
        <BlockFormControlLabel
          control={
            <Checkbox
              name="pack_set_enabled"
              checked={packSet.enabled}
              onChange={(event) => updatePackSet({ enabled: event.target.checked })}
            />
          }
          label="Pack-Size Auto-Save: match net weight to configured pack targets?"
        />
        <TextField
          name="pack_set_targets_g"
          label={`Pack targets (g, up to ${packMax})`}
          fullWidth
          variant="outlined"
          value={shownTargets}
          onChange={handleTargetsChange}
          margin="normal"
          error={Boolean(targetsError)}
          helperText={
            targetsError ??
            'Comma-separated whole grams, e.g. 500, 1000, 2000. Ignored when a Hi-Lo product is picked.'
          }
        />
        <TextField
          name="pack_set_tolerance_g"
          label="Pack tolerance (+/- g)"
          fullWidth
          variant="outlined"
          type="number"
          value={packSet.tolerance_g >= 0 ? numberValue(packSet.tolerance_g) : ''}
          onChange={handleToleranceChange}
          margin="normal"
          error={Boolean(toleranceError)}
          helperText={toleranceError ?? 'Allowed +/- grams around the nearest target for Auto save.'}
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
    <>
      <SectionContent title="W-Soft settings" titleGutter>
        {content()}
      </SectionContent>
      {data?.relay_available && <WSoftRelayTest hiloOn={data.relay_enabled} />}
      <WSoftPinForm />
      <WSoftWifiUpdateForm />
    </>
  );
};

export default WSoftSettingsForm;
