export interface LauncherState {
    installed: boolean;
    dllPath: string;
    dllVersion: string;
    updateStatus: '' | 'checking' | 'current' | 'outdated' | 'unknown';
    latestVersion: string;
    launcherUpdateAvailable: boolean;
    found: boolean;
    scanning: boolean;
    error: string;
    characters: string[];
    selected: number;
}

export type InstallEvent =
    | { phase: 'release'; changelog: string }
    | { phase: 'progress'; percent: number }
    | { phase: 'done'; ok: boolean; message: string };

export interface Result {
    ok: boolean;
    message: string;
}

export interface ChoiceEvent {
    title: string;
    message: string;
    options: string[];
}
