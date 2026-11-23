import type { ReactNode } from 'react';
import { call } from '../bridge';
import appIcon from '../assets/app-icon.png';

export const TITLE_BAR_HEIGHT = 28;
const TITLE_ICON_SIZE = 20;
const TITLE_ICON_INSET = (TITLE_BAR_HEIGHT - TITLE_ICON_SIZE) / 2;

export function TitleBar({ title, action }: { title: string; action?: ReactNode }) {
    return (
        <div className="title-bar flex shrink-0 items-center border-b border-border bg-surface" style={{ height: TITLE_BAR_HEIGHT }}>
            <div
                className="flex h-full min-w-0 flex-1 items-center pr-2.5"
                style={{ paddingLeft: TITLE_ICON_INSET }}
                onMouseDown={(event) => {
                    if (event.button === 0) {
                        call('beginMove');
                    }
                }}
            >
                <img src={appIcon} alt="" width={TITLE_ICON_SIZE} height={TITLE_ICON_SIZE} className="mr-1.5 shrink-0" draggable={false} />
                <span className="truncate text-xs">{title}</span>
            </div>
            {action}
            <TitleButton label="Close" danger onClick={() => call('close')}>
                <path d="M1.5 1.5l7 7M8.5 1.5l-7 7" />
            </TitleButton>
        </div>
    );
}

export function UpdateAvailableButton({ onClick }: { onClick: () => void }) {
    return (
        <button
            type="button"
            title="Download and install the latest GWToolbox"
            onClick={onClick}
            className="update-available mr-1.5 h-5 cursor-pointer self-center rounded border border-accent px-1.5 text-[10px] leading-none text-accent hover:bg-accent hover:text-white"
        >
            Update Available
        </button>
    );
}

function TitleButton({ label, danger, onClick, children }: { label: string; danger?: boolean; onClick: () => void; children: ReactNode }) {
    return (
        <button
            type="button"
            aria-label={label}
            title={label}
            onClick={onClick}
            className={`title-button flex h-full w-10 cursor-default items-center justify-center border-0 bg-transparent p-0 text-[#111] ${danger ? 'hover:bg-danger hover:text-white' : 'hover:bg-surface-hover'}`}
        >
            <svg width="10" height="10" viewBox="0 0 10 10" fill="none" stroke="currentColor" strokeWidth="1">
                {children}
            </svg>
        </button>
    );
}
