import { useEffect, useState } from 'react';
import { call, on } from './bridge';
import { Button } from './components/Button';
import { InstallProgressView, type InstallProgress, type InstallTarget } from './components/InstallProgress';
import { TitleBar } from './components/TitleBar';
import type { InstallEvent, Result } from './types';

interface Release {
    changelog: string;
    error: string;
}

export type InstallMode = 'install' | 'update';

const MODE_TEXT = {
    install: {
        title: 'GWToolbox - Install',
        target: 'dll' as InstallTarget,
        location: 'Where should GWToolbox be installed?',
        note: 'The GWToolbox DLL is saved in this folder.',
        action: 'Install',
    },
    update: {
        title: 'GWToolbox - Update',
        target: 'launcher' as InstallTarget,
        location: 'GWToolbox will be updated in this folder.',
        note: 'GWToolbox restarts after the update is installed.',
        action: 'Update',
    },
} as const;

export function InstallApp({ mode = 'install' }: { mode?: InstallMode }) {
    const text = MODE_TEXT[mode];
    const [release, setRelease] = useState<Release | null>(null);
    const [location, setLocation] = useState<string | null>(null);
    const [error, setError] = useState('');
    const [progress, setProgress] = useState<InstallProgress | null>(null);

    useEffect(() => on('install', (data) => {
        const event = data as InstallEvent & { error?: string };
        switch (event.phase) {
            case 'release':
                setRelease({ changelog: event.changelog ?? '', error: event.error ?? '' });
                break;
            case 'progress':
                setProgress((previous) => ({ ...(previous ?? { percent: null, done: false, ok: false, message: '' }), percent: event.percent }));
                break;
            case 'done':
                setProgress((previous) => ({
                    ...(previous ?? { percent: null, done: false, ok: false, message: '' }),
                    done: true,
                    ok: event.ok,
                    message: event.message,
                    percent: event.ok ? 100 : previous?.percent ?? null,
                }));
                break;
        }
    }), []);

    useEffect(() => {
        call('ready');
        call<{ suggested: string }>('getState').then((state) => setLocation(state.suggested));
    }, []);

    const install = async () => {
        if (!location) {
            return;
        }
        const result = await call<Result>('install', location);
        if (result.ok) {
            setError('');
            setProgress({ percent: null, done: false, ok: false, message: '' });
        } else if (result.message) {
            setError(result.message);
        }
    };

    return (
        <div className="window-frame flex h-full flex-col border border-border bg-white">
            <TitleBar title={text.title} />
            <main className="window-content flex min-h-0 flex-1 flex-col">
                <section className="flex min-h-0 flex-1 flex-col gap-1.5 p-3">
                    <h2 className="m-0 text-sm">Release notes</h2>
                    <textarea
                        aria-label="Release notes"
                        readOnly
                        className="changelog selectable m-0 min-h-0 flex-1 resize-none overflow-auto rounded border border-border bg-white p-2 font-app text-xs leading-relaxed"
                        value={release === null ? 'Loading release notes...' : release.error || release.changelog}
                    />
                </section>
                <section className="flex flex-col gap-3 border-t border-divider p-3">
                    {progress ? (
                        <InstallProgressView progress={progress} target={text.target} />
                    ) : (
                        <>
                            <div className="flex flex-col gap-1">
                                <h2 className="m-0 text-sm">{text.location}</h2>
                                <p className="note m-0">{text.note}</p>
                            </div>
                            {mode === 'install' ? (
                                <div className="flex min-w-0 gap-2">
                                    <input
                                        aria-label="Install folder"
                                        className="min-w-0 flex-1 rounded border border-border bg-white px-2 py-[5px] text-[13px] text-[#111]"
                                        value={location ?? ''}
                                        onChange={(event) => setLocation(event.target.value)}
                                    />
                                    <Button
                                        onClick={async () => {
                                            const picked = await call<{ path: string }>('chooseFolder');
                                            if (picked.path) {
                                                setLocation(picked.path);
                                            }
                                        }}
                                    >
                                        Browse...
                                    </Button>
                                </div>
                            ) : (
                                <p className="changelog selectable m-0 break-all rounded border border-border bg-white p-2 font-app text-xs">{location ?? ''}</p>
                            )}
                            {error && <p className="error m-0 whitespace-pre-line text-xs">{error}</p>}
                            <div className="buttons flex justify-end gap-2 pt-1.5">
                                <Button onClick={() => call('close')}>Cancel</Button>
                                <Button className="primary" disabled={!location?.trim()} onClick={install}>
                                    {text.action}
                                </Button>
                            </div>
                        </>
                    )}
                </section>
            </main>
        </div>
    );
}
