import { FC, useContext } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import SerialMonitor from '../examples/serial/SerialMonitor';
import Diagnostics from '../examples/diagnostics/Diagnostics';
import WeightForwarder from '../examples/weightforwarder/WeightForwarder';
import RemoteWeightMonitor from '../examples/remoteweight/RemoteWeightMonitor';
import LcdDisplay from '../examples/lcddisplay/LcdDisplay';
import WSoft from './wsoft/WSoft';
import WSoftHubPage from './wsoft/WSoftHubPage';
import { FeaturesContext } from '../contexts/features';

const ProjectRouting: FC = () => {
  const { features } = useContext(FeaturesContext);
  return (
    <Routes>
      {
        // Default route: the W-Soft page on a W-Soft board, the LED example elsewhere
      }
      <Route path="/*" element={<Navigate to={features.wsoft ? 'wsoft' : 'led-example/information'} />} />
      {
        // LED Example project routes
      }
      <Route path="led-example/*" element={<LedExample />} />
      {
        // Serial Monitor project routes
      }
      <Route path="serial/*" element={<SerialMonitor />} />
      {
        // UART Diagnostics project routes
      }
      <Route path="diagnostics/*" element={<Diagnostics />} />
      {
        // Weight Forwarder project routes
      }
      <Route path="weightforwarder/*" element={<WeightForwarder />} />
      {
        // Remote Weight receiver (weight pulled from another ESP)
      }
      <Route path="remote-weight/*" element={<RemoteWeightMonitor />} />
      {
        // Character-LCD display device
      }
      <Route path="lcd-display/*" element={<LcdDisplay />} />
      {
        // W-Soft weigh screen (Waveshare 4.3 + DJB over RS485, env wsoft_waveshare43)
      }
      <Route path="wsoft/*" element={<WSoft />} />
      {
        // W-Soft Hub link (admin), its own sidebar item
      }
      {features.wsoft && <Route path="hub" element={<WSoftHubPage />} />}
    </Routes>
  );
};

export default ProjectRouting;
