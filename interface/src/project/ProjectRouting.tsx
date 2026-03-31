import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import Growbox from '../examples/growbox/Growbox';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      <Route path="/*" element={<Navigate to="growbox/dashboard" />} />
      <Route path="led-example/*" element={<LedExample />} />
      <Route path="growbox/*" element={<Growbox />} />
    </Routes>
  );
};

export default ProjectRouting;
