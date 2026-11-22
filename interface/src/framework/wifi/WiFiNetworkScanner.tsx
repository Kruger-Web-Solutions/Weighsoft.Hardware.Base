import { useEffect, FC, useState, useCallback, useRef } from 'react';
import { useSnackbar } from 'notistack';

import { Alert, Button, } from '@mui/material';
import PermScanWifiIcon from '@mui/icons-material/PermScanWifi';

import * as WiFiApi from "../../api/wifi";
import { WiFiNetwork, WiFiNetworkList } from '../../types';
import { ButtonRow, FormLoader, SectionContent } from '../../components';
import { extractErrorMessage } from '../../utils';

import WiFiNetworkSelector from './WiFiNetworkSelector';

// 20 polls × 750 ms = 15 s. Backend scan with 300 ms-per-channel dwell takes
// ~4 s, so 15 s gives plenty of headroom even when the radio is busy.
const NUM_POLLS = 20;
const POLLING_FREQUENCY = 750;

const compareNetworks = (network1: WiFiNetwork, network2: WiFiNetwork) => {
  if (network1.rssi < network2.rssi)
    return 1;
  if (network1.rssi > network2.rssi)
    return -1;
  return 0;
};

const WiFiNetworkScanner: FC = () => {

  const { enqueueSnackbar } = useSnackbar();

  const pollCount = useRef(0);
  const [networkList, setNetworkList] = useState<WiFiNetworkList>();
  const [errorMessage, setErrorMessage] = useState<string>();

  const finishedWithError = useCallback((message: string) => {
    enqueueSnackbar(message, { variant: 'error' });
    setNetworkList(undefined);
    setErrorMessage(message);
  }, [enqueueSnackbar]);

  const pollNetworkList = useCallback(async () => {
    // Each poll uses one of NUM_POLLS. Returns true while there is time to try again.
    const pollAgain = () => {
      const completedPollCount = pollCount.current + 1;
      if (completedPollCount >= NUM_POLLS) {
        return false;
      }
      pollCount.current = completedPollCount;
      setTimeout(pollNetworkList, POLLING_FREQUENCY);
      return true;
    };
    try {
      const response = await WiFiApi.listNetworks();
      if (response.status === 202) {
        if (!pollAgain()) {
          finishedWithError("Device did not return network list in timely manner");
        }
      } else {
        const newNetworkList = response.data;
        newNetworkList.networks.sort(compareNetworks);
        setNetworkList(newNetworkList);
      }
    } catch (error: any) {
      // While the board scans, its radio leaves the hotspot's channel, so a phone on the
      // hotspot loses some requests. One lost poll used to end the whole scan with an
      // error (14 Nov 2026: "it doesn't always pick up the Wi-Fi"). Keep polling instead;
      // a 401/403 is a real answer and still ends it.
      const status = error?.response?.status;
      if ((status === 401 || status === 403) || !pollAgain()) {
        finishedWithError(extractErrorMessage(error, 'Problem listing WiFi networks'));
      }
    }
  }, [finishedWithError]);

  const startNetworkScan = useCallback(async () => {
    pollCount.current = 0;
    setNetworkList(undefined);
    setErrorMessage(undefined);
    try {
      await WiFiApi.scanNetworks();
      setTimeout(pollNetworkList, POLLING_FREQUENCY);
    } catch (error: any) {
      finishedWithError(extractErrorMessage(error, 'Problem scanning for WiFi networks'));
    }
  }, [finishedWithError, pollNetworkList]);

  useEffect(() => { startNetworkScan(); }, [startNetworkScan]);

  const renderNetworkScanner = () => {
    if (!networkList) {
      return (<FormLoader message="Scanning&hellip;" errorMessage={errorMessage} />);
    }
    if (networkList.networks.length === 0) {
      return (
        <Alert severity="info" sx={{ mt: 2 }}>
          No WiFi networks found. The radio occasionally misses networks while connected to WiFi —
          click <strong>Scan again</strong> to retry.
        </Alert>
      );
    }
    return (
      <WiFiNetworkSelector networkList={networkList} />
    );
  };

  return (
    <SectionContent title="Network Scanner">
      {renderNetworkScanner()}
      <ButtonRow pt={1}>
        <Button
          startIcon={<PermScanWifiIcon />}
          variant="contained"
          color="secondary"
          onClick={startNetworkScan}
          disabled={!errorMessage && !networkList}
        >
          Scan again&hellip;
        </Button>
      </ButtonRow>
    </SectionContent>
  );

};

export default WiFiNetworkScanner;
