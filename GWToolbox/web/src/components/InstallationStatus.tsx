import { useState } from 'react';
import { call } from '../bridge';
import type { LauncherState, Result } from '../types';
import { Button } from './Button';

function UpdateStatus({ state }: { state: LauncherState }) {
    switch (state.updateStatus) {
        case 'checking':
            return <span className="note">Checking for updates...</span>;
        case 'current':
            return <span className="note">Up to date</span>;
        case 'outdated':
            return (
                <span className="warning note">Update available: {state.latestVersion}</span>
            );
        case 'unknown':
            return <span className="note">Couldn't check for updates</span>;
        default:
            return null;
    }
}

export function InstallationStatus({ state }: { state: LauncherState }) {
    const [message, setMessage] = useState('');
    const folder = state.dllPath.slice(0, Math.max(state.dllPath.lastIndexOf('/'), 0));

    return (
        <section className="installation flex flex-col gap-1.5 border-t border-divider px-3 py-2.5">
            <div className="flex items-center gap-2">
                <span className="text-xs font-medium">GWToolboxdll.dll</span>
                <span className={`note ${state.installed ? '' : 'warning'}`}>
                    {state.installed ? (state.dllVersion ? `Version ${state.dllVersion}` : 'Found') : 'Not found'}
                </span>
            </div>
            <p className="note m-0 truncate" title={state.dllPath}>
                {state.installed ? state.dllPath : 'Install GWToolbox, or browse to an existing GWToolboxdll.dll.'}
            </p>
            {state.installed && <UpdateStatus state={state} />}
            {message && <p className="error m-0 text-xs">{message}</p>}
            {(!state.installed || state.updateStatus === 'outdated') && (
                <div className="flex justify-end gap-2 pt-0.5">
                    {!state.installed && (
                        <Button
                            onClick={async () => {
                                const result = await call<Result>('browseDll');
                                setMessage(result.ok ? '' : result.message);
                            }}
                        >
                            Browse...
                        </Button>
                    )}
                    <Button
                        className="primary"
                        onClick={() => call('openInstall', state.installed ? folder : '')}
                    >
                        {state.installed ? 'Update' : 'Install'}
                    </Button>
                </div>
            )}
        </section>
    );
}
