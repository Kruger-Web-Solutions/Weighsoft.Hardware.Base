import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LedExample from '../examples/led/LedExample';
import RelayBoard from '../examples/relay/RelayBoard';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      <Route path="/*" element={<Navigate to="relay-board/twin" />} />
      <Route path="relay-board/*" element={<RelayBoard />} />
      <Route path="led-example/*" element={<LedExample />} />
    </Routes>
  );
};

export default ProjectRouting;
