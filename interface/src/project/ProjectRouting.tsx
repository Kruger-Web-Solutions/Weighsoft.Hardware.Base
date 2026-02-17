import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import SerialMonitor from '../examples/serial/SerialMonitor';
import Diagnostics from '../examples/diagnostics/Diagnostics';
import WeightForwarder from '../examples/weightforwarder/WeightForwarder';

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
    </Routes>
  );
};

export default ProjectRouting;
