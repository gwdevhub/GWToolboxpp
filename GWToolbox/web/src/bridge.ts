type Pending = { resolve: (value: unknown) => void; reject: (error: Error) => void };
type Listener = (data: unknown) => void;

const pending = new Map<number, Pending>();
const listeners = new Map<string, Listener[]>();
let nextId = 1;

const channel = () => (window as any).chrome?.webview ?? (window as any).webkit.messageHandlers.host;
const post = (message: object) => channel().postMessage(JSON.stringify(message));

(window as any).hostReceive = (message: any) => {
    if (message.event !== undefined) {
        for (const listener of listeners.get(message.event) ?? []) {
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

export function call<T = unknown>(method: string, ...args: unknown[]): Promise<T> {
    return new Promise((resolve, reject) => {
        const id = nextId++;
        pending.set(id, { resolve: resolve as (value: unknown) => void, reject });
        post({ id, method, args });
    });
}

export function on(event: string, listener: Listener): () => void {
    listeners.set(event, [...(listeners.get(event) ?? []), listener]);
    return () => listeners.set(event, (listeners.get(event) ?? []).filter((existing) => existing !== listener));
}
