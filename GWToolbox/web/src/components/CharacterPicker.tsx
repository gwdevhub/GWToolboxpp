import { useState } from 'react';
import { call } from '../bridge';
import type { LauncherState } from '../types';
import { Button } from './Button';

export function CharacterPicker({ state }: { state: LauncherState }) {
    const [selected, setSelected] = useState(state.selected);

    const hasCharacters = state.found && state.characters.length > 0;
    const placeholder = state.scanning ? 'Scanning...' : 'Nothing found';
    const launchable = hasCharacters && !state.scanning && state.installed && selected >= 0 && selected < state.characters.length;

    return (
        <section className="picker flex flex-col gap-2 p-3">
            <fieldset className="relative m-0 flex w-full min-w-0 flex-col rounded border border-border px-2.5 py-3">
                <legend className="note absolute -top-[7px] left-2 bg-white px-1">Select Character</legend>
                <div className="flex min-w-0 gap-2">
                    <select
                        aria-label="Character"
                        className="min-w-0 flex-1 truncate rounded border border-border bg-white px-2 py-[5px] text-[13px] text-[#111]"
                        value={hasCharacters ? selected : ''}
                        disabled={!hasCharacters || state.scanning}
                        onChange={(event) => setSelected(Number(event.target.value))}
                    >
                        {!hasCharacters && <option value="">{placeholder}</option>}
                        {state.characters.map((label, index) => (
                            <option key={index} value={index}>
                                {label}
                            </option>
                        ))}
                    </select>
                    <Button className="primary shrink-0" disabled={!launchable} onClick={() => call('launch', selected)}>
                        Launch
                    </Button>
                </div>
            </fieldset>
            {!state.installed && <p className="warning m-0 text-xs">Install GWToolbox to launch a character.</p>}
            {state.error && <p className="warning m-0 whitespace-pre-line text-[11px]">{state.error}</p>}
        </section>
    );
}
