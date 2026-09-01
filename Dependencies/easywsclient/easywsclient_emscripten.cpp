// easywsclient — Emscripten backend
//
// The public API (see easywsclient.hpp) is preserved so callers don't change.
// Internally we use the browser's native WebSocket via emscripten/websocket.h —
// the browser already speaks the WebSocket protocol (framing + TLS), so unlike
// the native backend there is no handshake/socket work to do here, only
// wiring the browser's async callbacks into the same poll()/dispatch() shape
// the native (WinHTTP) backend presents.

#include <atomic>
#include <cstdio>
#include <mutex>
#include <queue>
#include <string>

#include <emscripten/websocket.h>

#include "easywsclient.hpp"

namespace { // private module-only namespace

    int log_error(const char* format, ...)
    {
        va_list vl;
        va_start(vl, format);
        const int written = vfprintf(stderr, format, vl);
        va_end(vl);
        return written;
    }

    class _DummyWebSocket : public easywsclient::WebSocket {
    public:
        void poll(int /*timeout*/) override {}
        void send(const std::string& /*message*/) override {}
        void sendPing() override {}
        void close() override {}
        void _dispatch(Callback& /*callable*/) override {}
        readyStateValues getReadyState() const override { return CLOSED; }
    };

    class _EmscriptenWebSocket : public easywsclient::WebSocket {
    public:
        explicit _EmscriptenWebSocket(EMSCRIPTEN_WEBSOCKET_T socket)
            : m_Socket(socket)
            , m_State(CONNECTING)
        {
            emscripten_websocket_set_onopen_callback(m_Socket, this, &_EmscriptenWebSocket::OnOpen);
            emscripten_websocket_set_onmessage_callback(m_Socket, this, &_EmscriptenWebSocket::OnMessage);
            emscripten_websocket_set_onerror_callback(m_Socket, this, &_EmscriptenWebSocket::OnError);
            emscripten_websocket_set_onclose_callback(m_Socket, this, &_EmscriptenWebSocket::OnClose);
        }

        ~_EmscriptenWebSocket() override
        {
            emscripten_websocket_delete(m_Socket);
        }

        readyStateValues getReadyState() const override
        {
            return m_State.load(std::memory_order_acquire);
        }

        void poll(int /*timeout*/) override
        {
            // Callbacks are driven by the browser's own event loop, not by us -
            // nothing to pump here. The timeout parameter exists only for the
            // native backend's blocking-reader-thread wakeup and has no
            // equivalent on this backend.
        }

        void sendPing() override
        {
            // The browser's WebSocket implementation handles PING/PONG frames
            // transparently. Nothing to do.
        }

        void send(const std::string& message) override
        {
            if (m_State.load() != OPEN) return;
            emscripten_websocket_send_utf8_text(m_Socket, message.c_str());
        }

        void close() override
        {
            readyStateValues expected = OPEN;
            if (!m_State.compare_exchange_strong(expected, CLOSING)) {
                return;
            }
            emscripten_websocket_close(m_Socket, 1000, "");
        }

        void _dispatch(Callback& callable) override
        {
            std::queue<std::string> drain;
            {
                std::scoped_lock lk(m_QueueMutex);
                drain.swap(m_IncomingQueue);
            }
            while (!drain.empty()) {
                callable(drain.front());
                drain.pop();
            }
        }

    private:
        static EM_BOOL OnOpen(int /*eventType*/, const EmscriptenWebSocketOpenEvent* e, void* userData)
        {
            auto* self = static_cast<_EmscriptenWebSocket*>(userData);
            self->m_State.store(OPEN, std::memory_order_release);
            return EM_TRUE;
        }

        static EM_BOOL OnMessage(int /*eventType*/, const EmscriptenWebSocketMessageEvent* e, void* userData)
        {
            auto* self = static_cast<_EmscriptenWebSocket*>(userData);
            if (!e->isText) return EM_TRUE; // binary frames aren't used by any current caller
            std::scoped_lock lk(self->m_QueueMutex);
            self->m_IncomingQueue.emplace(reinterpret_cast<const char*>(e->data), e->numBytes ? e->numBytes - 1 : 0); // drop the glue's trailing NUL
            return EM_TRUE;
        }

        static EM_BOOL OnError(int /*eventType*/, const EmscriptenWebSocketErrorEvent* /*e*/, void* userData)
        {
            auto* self = static_cast<_EmscriptenWebSocket*>(userData);
            log_error("easywsclient(emscripten): socket error\n");
            self->m_State.store(CLOSED, std::memory_order_release);
            return EM_TRUE;
        }

        static EM_BOOL OnClose(int /*eventType*/, const EmscriptenWebSocketCloseEvent* /*e*/, void* userData)
        {
            auto* self = static_cast<_EmscriptenWebSocket*>(userData);
            self->m_State.store(CLOSED, std::memory_order_release);
            return EM_TRUE;
        }

        EMSCRIPTEN_WEBSOCKET_T m_Socket;
        std::atomic<readyStateValues> m_State;

        std::queue<std::string> m_IncomingQueue;
        std::mutex m_QueueMutex;
    };

    easywsclient::WebSocket::pointer from_url(
        const std::string& url,
        easywsclient::HeaderKeyValuePair /*additional_headers*/,
        const std::string& /*origin*/)
    {
        // Browsers don't allow custom headers or Origin overrides on the
        // WebSocket handshake at all (security-sandboxed) - additional_headers
        // and origin are silently ignored here, unlike the native backend
        // which sends them itself. Any caller relying on custom auth headers
        // needs to move that auth into the URL (query string / subprotocol)
        // for this backend to work.
        if (!emscripten_websocket_is_supported()) {
            log_error("ERROR: WebSocket not supported in this environment\n");
            return nullptr;
        }

        EmscriptenWebSocketCreateAttributes attr;
        emscripten_websocket_init_create_attributes(&attr);
        attr.url = url.c_str();

        EMSCRIPTEN_WEBSOCKET_T socket = emscripten_websocket_new(&attr);
        if (socket <= 0) {
            log_error("ERROR: emscripten_websocket_new failed for: %s\n", url.c_str());
            return nullptr;
        }

        return new _EmscriptenWebSocket(socket);
    }

} // namespace

namespace easywsclient {

    WebSocket::pointer WebSocket::create_dummy()
    {
        static pointer dummy = pointer(new _DummyWebSocket);
        return dummy;
    }

    WebSocket::pointer WebSocket::from_url(const std::string& url,
                                            const HeaderKeyValuePair& additional_headers,
                                            const std::string& origin)
    {
        return ::from_url(url, additional_headers, origin);
    }

    WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url,
                                                    const HeaderKeyValuePair& additional_headers,
                                                    const std::string& origin)
    {
        // Masking is mandatory for browser-originated frames and handled
        // internally by the browser; the parameter is accepted only for API
        // compatibility with the native backend.
        return ::from_url(url, additional_headers, origin);
    }

} // namespace easywsclient
