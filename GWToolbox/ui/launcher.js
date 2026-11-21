const launcher = (() => {
    const pending = new Map();
    const listeners = new Map();
    let nextId = 1;

    const channel = () => (window.chrome && window.chrome.webview) || window.webkit.messageHandlers.host;
    const post = (message) => channel().postMessage(JSON.stringify(message));

    window.hostReceive = (message) => {
        if (message.event !== undefined) {
            for (const listener of listeners.get(message.event) || []) {
                listener(message.data);
            }
            return;
        }
        const reply = pending.get(message.id);
        if (!reply) {
            return;
        }
        pending.delete(message.id);
        if (message.error !== undefined) {
            reply.reject(new Error(message.error));
        } else {
            reply.resolve(message.result);
        }
    };

    window.addEventListener('error', (event) => post({id: 0, method: 'frontendError', args: [event.message]}));

    return {
        call: (method, ...args) => new Promise((resolve, reject) => {
            const id = nextId++;
            pending.set(id, {resolve, reject});
            post({id, method, args});
        }),
        on: (event, listener) => listeners.set(event, [...(listeners.get(event) || []), listener]),
        wireTitleBar: () => {
            document.getElementById('titlebar').addEventListener('mousedown', (event) => {
                if (event.button === 0 && !event.target.closest('button')) {
                    launcher.call('beginMove');
                }
            });
            document.getElementById('close').addEventListener('click', () => launcher.call('close'));
        },
    };
})();
