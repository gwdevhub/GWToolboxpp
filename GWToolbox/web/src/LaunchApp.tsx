import { useCallback, useEffect, useState } from 'react';
import { useAutosize } from './autosize';
import { call } from './bridge';
import { Button } from './components/Button';
import { CharacterPicker } from './components/CharacterPicker';
import { InstallationStatus } from './components/InstallationStatus';
import { TitleBar, UpdateAvailableButton } from './components/TitleBar';
import type { LauncherState } from './types';

export function LaunchApp() {
    const [state, setState] = useState<LauncherState | null>(null);
    const [root, setRoot] = useState<HTMLDivElement | null>(null);
    useAutosize(root);

    useEffect(() => {
        call('ready');
    }, []);

    const refresh = useCallback(async () => setState(await call<LauncherState>('getState')), []);

    useEffect(() => {
        refresh();
        const timer = setInterval(refresh, 500);
        return () => clearInterval(timer);
    }, [refresh]);

    return (
        <div className="window-frame flex h-full flex-col border border-border bg-white">
            <TitleBar
                title="GWToolbox - Launch"
                action={state?.launcherUpdateAvailable ? <UpdateAvailableButton onClick={() => call('updateLauncher')} /> : undefined}
            />
            <main className="window-content flex min-h-0 flex-1 flex-col">
                <div ref={setRoot} className="flex flex-col">
                    {state === null ? <p className="note p-3">Starting...</p> : <CharacterPicker state={state} />}
                    {state !== null && <InstallationStatus state={state} />}
                    <div className="flex justify-between border-t border-divider px-3 py-2">
                        <Button disabled={state?.scanning ?? false} onClick={() => call('retry').then(refresh)}>
                            {state?.scanning ? 'Scanning...' : 'Re-scan'}
                        </Button>
                        <Button onClick={() => call('settings')}>Settings...</Button>
                    </div>
                </div>
            </main>
        </div>
    );
}
