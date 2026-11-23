import { useEffect, useState } from 'react';
import { call } from './bridge';
import { Button } from './components/Button';
import { TitleBar } from './components/TitleBar';
import type { ChoiceEvent } from './types';

export function ChoiceApp() {
    const [choice, setChoice] = useState<ChoiceEvent | null>(null);
    const [root, setRoot] = useState<HTMLDivElement | null>(null);

    useEffect(() => {
        call<ChoiceEvent>('choice').then(setChoice);
    }, []);

    useEffect(() => {
        call('ready');
    }, []);

    useEffect(() => {
        if (!root) {
            return;
        }
        const observer = new ResizeObserver(() => {
            const frame = document.querySelector('.window-frame')?.getBoundingClientRect();
            const box = root.getBoundingClientRect();
            if (box.width === 0 || box.height === 0) {
                return;
            }
            const content = document.querySelector('.window-content')?.getBoundingClientRect();
            const extraWidth = frame && content ? frame.width - content.width : 0;
            const extraHeight = frame && content ? frame.height - content.height : 0;
            call('fitToPage', Math.ceil(box.width + extraWidth), Math.ceil(box.height + extraHeight));
        });
        observer.observe(root);
        return () => observer.disconnect();
    }, [root]);

    return (
        <div className="window-frame flex h-full flex-col border border-border bg-white">
            <TitleBar title={choice?.title ?? 'GWToolbox'} />
            <main className="window-content flex min-h-0 flex-1 flex-col">
                <div ref={setRoot} className="flex flex-col">
                    {choice && (
                        <section className="choice flex flex-col gap-3 p-3">
                            <p className="m-0 whitespace-pre-line text-xs leading-relaxed">{choice.message}</p>
                            <div className="buttons mt-auto flex justify-end gap-2 pt-1.5">
                                {choice.options.map((option, index) => (
                                    <Button key={option} className={index === 0 ? 'primary' : ''} onClick={() => call('answer', index)}>
                                        {option}
                                    </Button>
                                ))}
                            </div>
                        </section>
                    )}
                </div>
            </main>
        </div>
    );
}
