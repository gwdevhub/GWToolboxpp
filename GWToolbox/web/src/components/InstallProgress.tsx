import { call } from '../bridge';
import { Button } from './Button';
import { ProgressBar } from './ProgressBar';

export interface InstallProgress {
    percent: number | null;
    done: boolean;
    ok: boolean;
    message: string;
}

export type InstallTarget = 'dll' | 'launcher';

const TARGET_TEXT = {
    dll: {
        file: 'GWToolboxdll.dll',
        heading: 'Installing GWToolbox',
        done: 'Download complete. Click Done to close.',
    },
    launcher: {
        file: 'GWToolbox.exe',
        heading: 'Updating GWToolbox',
        done: 'Update installed. Click Done to restart GWToolbox.',
    },
} as const;

export function InstallProgressView({ progress, target }: { progress: InstallProgress; target: InstallTarget }) {
    const text = TARGET_TEXT[target];
    const failed = progress.done && !progress.ok;
    const status = !progress.done ? `Downloading ${text.file}...` : progress.ok ? text.done : progress.message;

    return (
        <div className="install flex flex-col gap-2.5">
            <h2 className="m-0 text-sm">{text.heading}</h2>
            <ProgressBar fraction={progress.percent === null ? null : progress.percent / 100} label={`${text.file} download`} />
            <p className={`status m-0 text-xs ${failed ? 'error' : 'note'}`}>{status}</p>
            {progress.done && (
                <div className="buttons flex justify-end pt-1.5">
                    <Button className="primary" onClick={() => call('close')}>
                        Done
                    </Button>
                </div>
            )}
        </div>
    );
}
