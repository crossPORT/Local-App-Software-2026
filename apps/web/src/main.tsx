import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import { App } from './App';
import { LandingPage } from './components/LandingPage';
import { startBoothLogShipper } from './lib/booth_log_shipper';
import './index.css';

startBoothLogShipper();

function routePathname(): string {
  const path = window.location.pathname.replace(/\/+$/, '');
  return path || '/';
}

function Root() {
  const path = routePathname();
  if (path === '/' || path === '/landing') {
    return <LandingPage />;
  }
  return <App />;
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <Root />
  </StrictMode>,
);
