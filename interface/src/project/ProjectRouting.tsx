import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import Weighing from '../examples/weighing/Weighing';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      {
        // Default route -- LED example
      }
      <Route path="/*" element={<Navigate to="led-example/information" />} />
      {
        // LED Example project routes
      }
      <Route path="led-example/*" element={<LedExample />} />
      {
        // Weighing Board project routes
      }
      <Route path="weighing/*" element={<Weighing />} />
    </Routes>
  );
};

export default ProjectRouting;
