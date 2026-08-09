import { FC } from 'react';
import { Navigate, Routes, Route } from 'react-router-dom';

import LiveWeight from '../examples/liveweight/LiveWeight';
import RelayBoard from '../examples/relay/RelayBoard';

const ProjectRouting: FC = () => {
  return (
    <Routes>
      <Route path="/*" element={<Navigate to="relay-board/twin" />} />
      <Route path="relay-board/*" element={<RelayBoard />} />
      <Route path="live-weight/*" element={<LiveWeight />} />
    </Routes>
  );
};

export default ProjectRouting;
