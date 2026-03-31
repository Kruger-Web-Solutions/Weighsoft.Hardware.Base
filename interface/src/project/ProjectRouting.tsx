import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import Diagnostics from '../examples/diagnostics/Diagnostics';
import SerialWriter from '../examples/serialwriter/SerialWriter';
import SerialWriterForwarder from '../examples/serialwriterforwarder/SerialWriterForwarder';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      <Route path="/*" element={<Navigate to="serialwriter/information" />} />
      <Route path="led-example/*" element={<LedExample />} />
      <Route path="diagnostics/*" element={<Diagnostics />} />
      <Route path="serialwriter/*" element={<SerialWriter />} />
      <Route path="serialwriterforwarder/*" element={<SerialWriterForwarder />} />
    </Routes>
  );
};

export default ProjectRouting;
