import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import SerialMonitor from '../examples/serial/SerialMonitor';
import Diagnostics from '../examples/diagnostics/Diagnostics';
import WeightForwarder from '../examples/weightforwarder/WeightForwarder';
import SerialWriter from '../examples/serialwriter/SerialWriter';
import SerialWriterForwarder from '../examples/serialwriterforwarder/SerialWriterForwarder';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      <Route path="/*" element={<Navigate to="led-example/information" />} />
      <Route path="led-example/*" element={<LedExample />} />
      <Route path="serial/*" element={<SerialMonitor />} />
      <Route path="diagnostics/*" element={<Diagnostics />} />
      <Route path="weightforwarder/*" element={<WeightForwarder />} />
      <Route path="serialwriter/*" element={<SerialWriter />} />
      <Route path="serialwriterforwarder/*" element={<SerialWriterForwarder />} />
    </Routes>
  );
};

export default ProjectRouting;
