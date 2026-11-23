// Reports the element's size to the host, so an autosize window fits it. The size is in CSS pixels, which include the UI scale:
// the root's zoom is what the element's bounding box measures. The element must be sized by its content, not by the window.

import { useLayoutEffect } from 'react';
import { call } from './bridge';

export function useAutosize(element: HTMLElement | null) {
    useLayoutEffect(() => {
        if (!element) {
            return;
        }
        // The window is the element plus what the frame draws around the view: the title bar and the border.
        const frame = document.querySelector('.window-frame');
        const content = document.querySelector('.window-content');
        const report = () => {
            const box = element.getBoundingClientRect();
            if (box.width === 0 || box.height === 0) {
                return;
            }
            const outer = frame?.getBoundingClientRect();
            const inner = content?.getBoundingClientRect();
            const extraWidth = outer && inner ? outer.width - inner.width : 0;
            const extraHeight = outer && inner ? outer.height - inner.height : 0;
            call('fitToPage', Math.ceil(box.width + extraWidth), Math.ceil(box.height + extraHeight));
        };
        const observer = new ResizeObserver(report);
        observer.observe(element);
        // The title bar can arrive after the view, which changes what the frame takes without changing the element.
        if (content) {
            observer.observe(content);
        }
        return () => observer.disconnect();
    }, [element]);
}
