import { useEffect, useState } from 'react';
import { call } from './bridge';
import { Button } from './components/Button';
import { TitleBar } from './components/TitleBar';
import { useAutosize } from './autosize';
import type { Result } from './types';

interface SettingsState {
    noUpdate: boolean;
    dllPath: string;
    dllVersion: string;
    canRestartAsAdmin: boolean;
}

export function SettingsApp() {
    const [noUpdate, setNoUpdate] = useState<boolean | null>(null);
    const [dll, setDll] = useState<SettingsState | null>(null);
    const [message, setMessage] = useState('');
    const [root, setRoot] = useState<HTMLDivElement | null>(null);
    useAutosize(root);

    useEffect(() => {
        call('ready');
        const refresh = () => call<SettingsState>('getState').then((state) => {
            setNoUpdate(state.noUpdate);
            setDll(state);
        });
        refresh();
    }, []);

    return (
        <div className="window-frame flex h-full flex-col border border-border bg-white">
            <TitleBar title="GWToolbox - Settings" />
            <main className="window-content flex min-h-0 flex-1 flex-col">
                <div ref={setRoot} className="flex flex-col">
                    <section className="settings flex flex-col gap-2 p-3">
                        <label className="flex items-center gap-2 text-[13px]">
                            <input
                                type="checkbox"
                                disabled={noUpdate === null}
                                checked={noUpdate ?? false}
                                onChange={(event) => {
                                    setNoUpdate(event.target.checked);
                                    call('setOption', 'noUpdate', event.target.checked);
                                }}
                            />
                            Never check for update
                        </label>
                        <div className="flex flex-col gap-1 border-t border-divider pt-2">
                            <div className="flex items-center gap-2">
                                <span className="text-xs font-medium">GWToolboxdll.dll</span>
                                <span className="note">{dll?.dllVersion ? `Version ${dll.dllVersion}` : dll?.dllPath ? '' : 'Not found'}</span>
                            </div>
                            <p className="note m-0 truncate" title={dll?.dllPath ?? ''}>
                                {dll?.dllPath || 'No GWToolboxdll.dll chosen.'}
                            </p>
                            {message && <p className="error m-0 text-xs">{message}</p>}
                            <div className="flex justify-end pt-1">
                                <Button
                                    onClick={async () => {
                                        const result = await call<Result>('browseDll');
                                        setMessage(result.ok ? '' : result.message);
                                        if (result.ok) {
                                            setDll(await call<SettingsState>('getState'));
                                        }
                                    }}
                                >
                                    Browse...
                                </Button>
                            </div>
                        </div>
                        {dll?.canRestartAsAdmin && (
                            <div className="flex items-center justify-between gap-2 border-t border-divider pt-2">
                                <span className="note">Restart GWToolbox with administrator rights.</span>
                                <Button
                                    onClick={async () => {
                                        const result = await call<Result>('restartAsAdmin');
                                        if (!result.ok) {
                                            setMessage(result.message);
                                        }
                                    }}
                                >
                                    Restart as admin
                                </Button>
                            </div>
                        )}
                    </section>
                </div>
            </main>
        </div>
    );
}
