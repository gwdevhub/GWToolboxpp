import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import './app.css';
import { ChoiceApp } from './ChoiceApp';
import { InstallApp } from './InstallApp';
import { LaunchApp } from './LaunchApp';
import { SettingsApp } from './SettingsApp';

const view = document.body.dataset.view;

createRoot(document.getElementById('root')!).render(
    <StrictMode>
        {view === 'install' ? <InstallApp /> : view === 'update' ? <InstallApp mode="update" /> : view === 'choice' ? <ChoiceApp /> : view === 'settings' ? <SettingsApp /> : <LaunchApp />}
    </StrictMode>,
);
