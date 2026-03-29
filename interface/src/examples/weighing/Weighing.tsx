import React, { FC } from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { Tab } from '@mui/material';
import { RouterTabs, useRouterTab, useLayoutTitle } from '../../components';

import WeighingInfo from './WeighingInfo';
import WeighingConfig from './WeighingConfig';
import WeighingCalibration from './WeighingCalibration';
import WeighingRest from './WeighingRest';
import WeighingLive from './WeighingLive';
import WeighingAudit from './WeighingAudit';
import WeighingBle from './WeighingBle';

const Weighing: FC = () => {
  useLayoutTitle('Weighing');
  const { routerTab } = useRouterTab();

  return (
    <>
      <RouterTabs value={routerTab}>
        <Tab value="information"   label="Information" />
        <Tab value="configuration" label="Configuration" />
        <Tab value="calibration"   label="Calibration" />
        <Tab value="rest"          label="REST View" />
        <Tab value="stream"        label="Live Stream" />
        <Tab value="audit"         label="Audit Trail" />
        <Tab value="ble"           label="BLE" />
      </RouterTabs>
      <Routes>
        <Route path="information"   element={<WeighingInfo />} />
        <Route path="configuration" element={<WeighingConfig />} />
        <Route path="calibration"   element={<WeighingCalibration />} />
        <Route path="rest"          element={<WeighingRest />} />
        <Route path="stream"        element={<WeighingLive />} />
        <Route path="audit"         element={<WeighingAudit />} />
        <Route path="ble"           element={<WeighingBle />} />
        <Route path="/*"            element={<Navigate replace to="information" />} />
      </Routes>
    </>
  );
};

export default Weighing;
