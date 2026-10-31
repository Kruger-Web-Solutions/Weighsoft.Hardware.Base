import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import SerialMonitor from '../examples/serial/SerialMonitor';
import Diagnostics from '../examples/diagnostics/Diagnostics';
import WeightForwarder from '../examples/weightforwarder/WeightForwarder';
import RemoteWeightMonitor from '../examples/remoteweight/RemoteWeightMonitor';
import LcdDisplay from '../examples/lcddisplay/LcdDisplay';
import WSoft from './wsoft/WSoft';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      {
        // Default route for LED example project
      }
      <Route path="/*" element={<Navigate to="led-example/information" />} />
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
    </Routes>
  );
};

export default ProjectRouting;
