#pragma warning(disable: 4100)
#pragma warning(disable: 4244)
#pragma warning(disable: 4365)
#pragma warning(disable: 4456)
#pragma warning(disable: 4548)
#pragma warning(disable: 4701)
#pragma warning(disable: 5039)

#ifdef _WIN32
    #if defined(_MSC_VER) && !defined(_CRT_SECURE_NO_WARNINGS)
        #define _CRT_SECURE_NO_WARNINGS // _CRT_SECURE_NO_WARNINGS for sscanf errors in MSVC2013 Express
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #include <fcntl.h>
    #include <WinSock2.h>
    #include <WS2tcpip.h>
    #pragma comment( lib, "ws2_32" )
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/types.h>
    #include <io.h>
    #ifndef _SSIZE_T_DEFINED
        typedef int ssize_t;
        #define _SSIZE_T_DEFINED
    #endif
    #ifndef _SOCKET_T_DEFINED
        typedef SOCKET socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef snprintf
        #define snprintf _snprintf_s
    #endif
    #if _MSC_VER >=1600
        // vs2010 or later
        #include <stdint.h>
    #else
        typedef __int8 int8_t;
        typedef unsigned __int8 uint8_t;
        typedef __int32 int32_t;
        typedef unsigned __int32 uint32_t;
        typedef __int64 int64_t;
        typedef unsigned __int64 uint64_t;
    #endif
    #define socketerrno WSAGetLastError()
    #define SOCKET_EAGAIN_EINPROGRESS WSAEINPROGRESS
    #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
#else
    #include <fcntl.h>
    #include <netdb.h>
    #include <netinet/tcp.h>
    #include <stdio.h>
    #include <stdlib.h>
    #include <string.h>
    #include <sys/socket.h>
    #include <sys/time.h>
    #include <sys/types.h>
    #include <unistd.h>
    #include <stdint.h>
    #ifndef _SOCKET_T_DEFINED
        typedef int socket_t;
        #define _SOCKET_T_DEFINED
    #endif
    #ifndef INVALID_SOCKET
        #define INVALID_SOCKET (-1)
    #endif
    #ifndef SOCKET_ERROR
        #define SOCKET_ERROR   (-1)
    #endif
    #define closesocket(s) ::close(s)
    #include <errno.h>
    #define socketerrno errno
    #define SOCKET_EAGAIN_EINPROGRESS EAGAIN
    #define SOCKET_EWOULDBLOCK EWOULDBLOCK
#endif

#include <wolfssl/openssl/rand.h>
#include <wolfssl/openssl/ssl.h>
#include <wolfssl/openssl/err.h>
#include <vector>
#include <string>

#include "easywsclient.hpp"

namespace { // private module-only namespace

	struct ConnectionContext {
		socket_t sockfd;
		SSL *sslHandle;
		SSL_CTX *sslContext;
	};

    int log_error(const char* format, ...) {
        va_list vl;
        va_start(vl, format);
        int written = vfprintf(stderr, format, vl);
        va_end(vl);
        return written;
    }
    int print_error_callback(const char* err, size_t len, void* userdata) {
        return log_error("ssl error: %s", err);
    }

socket_t hostname_connect(const std::string& hostname, int port) {
    struct addrinfo hints;
    struct addrinfo *result;
    struct addrinfo *p;
    int ret;
    socket_t sockfd = INVALID_SOCKET;
    char sport[16];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    snprintf(sport, 16, "%d", port);
    if ((ret = getaddrinfo(hostname.c_str(), sport, &hints, &result)) != 0)
    {
        log_error("%d\n", ret);
      log_error("getaddrinfo: %s\n", gai_strerror(ret));
      return 1;
    }
    for(p = result; p != NULL; p = p->ai_next)
    {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) { continue; }
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) != SOCKET_ERROR) {
            break;
        }
        closesocket(sockfd);
        sockfd = INVALID_SOCKET;
    }
    freeaddrinfo(result);
    return sockfd;
}


int kWrite(ConnectionContext* ptConnCtx, const char* buf, int len, int flags) {
	if (ptConnCtx->sslHandle==NULL) {
		return ::send(ptConnCtx->sockfd, buf, len, flags);
	} else {
		return SSL_write(ptConnCtx->sslHandle, buf, len);
	}
}

int kRead(ConnectionContext* ptConnCtx, char* buf, int len, int flags) {
	if (ptConnCtx->sslHandle==NULL) {
		return ::recv(ptConnCtx->sockfd, buf, len, flags);
	} else {
		return SSL_read(ptConnCtx->sslHandle, buf, len);
	}
}

void klose(ConnectionContext* ptConnCtx) {
	if (ptConnCtx->sockfd) {
		closesocket(ptConnCtx->sockfd);
	}
	if (ptConnCtx->sslHandle) {
		SSL_shutdown(ptConnCtx->sslHandle);
		SSL_free(ptConnCtx->sslHandle);
	}
	if (ptConnCtx->sslContext) {
		SSL_CTX_free(ptConnCtx->sslContext);
	}
	free(ptConnCtx);
}

class _DummyWebSocket : public easywsclient::WebSocket
{
  public:
    void poll(int timeout) { }
    void send(const std::string& message) { }
    void sendPing() { }
    void close() { }
    void _dispatch(Callback & callable) { }
    readyStateValues getReadyState() const { return CLOSED; }
};



class _RealWebSocket : public easywsclient::WebSocket
{
  public:
    // http://tools.ietf.org/html/rfc6455#section-5.2  Base Framing Protocol
    //
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-------+-+-------------+-------------------------------+
    // |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
    // |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
    // |N|V|V|V|       |S|             |   (if payload len==126/127)   |
    // | |1|2|3|       |K|             |                               |
    // +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
    // |     Extended payload length continued, if payload len == 127  |
    // + - - - - - - - - - - - - - - - +-------------------------------+
    // |                               |Masking-key, if MASK set to 1  |
    // +-------------------------------+-------------------------------+
    // | Masking-key (continued)       |          Payload Data         |
    // +-------------------------------- - - - - - - - - - - - - - - - +
    // :                     Payload Data continued ...                :
    // + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
    // |                     Payload Data continued ...                |
    // +---------------------------------------------------------------+
    struct wsheader_type {
        unsigned header_size;
        bool fin;
        bool mask;
        enum opcode_type {
            CONTINUATION = 0x0,
            TEXT_FRAME = 0x1,
            BINARY_FRAME = 0x2,
            CLOSE = 8,
            PING = 9,
            PONG = 0xa,
        } opcode;
        int N0;
        uint64_t N;
        uint8_t masking_key[4];
    };

    std::vector<uint8_t> rxbuf;
    std::vector<uint8_t> txbuf;

    std::vector<uint8_t> receivedData;

    ConnectionContext* ptConnCtx;
    readyStateValues readyState;
    bool useMask;

    _RealWebSocket(ConnectionContext* connContext, bool useMask) : ptConnCtx(connContext), readyState(OPEN), useMask(useMask) {
    }

    readyStateValues getReadyState() const {
      return readyState;
    }

    void poll(int timeout) { // timeout in milliseconds
        if (readyState == CLOSED) {
            if (timeout > 0) {
                timeval tv = { timeout/1000, (timeout%1000) * 1000 };
                select(0, NULL, NULL, NULL, &tv);
            }
            return;
        }
        if (timeout > 0) {
            fd_set rfds;
            fd_set wfds;
            timeval tv = { timeout/1000, (timeout%1000) * 1000 };
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_SET(ptConnCtx->sockfd, &rfds);
            if (txbuf.size()) { FD_SET(ptConnCtx->sockfd, &wfds); }
            select(ptConnCtx->sockfd + 1, &rfds, &wfds, NULL, &tv);
        }
        while (true) {
            // FD_ISSET(0, &rfds) will be true
            int N = rxbuf.size();
            ssize_t ret;
            rxbuf.resize(N + 1500);
            ret = kRead(ptConnCtx, (char*)&rxbuf[0] + N, 1500, 0);
            if (false) { }
            else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                rxbuf.resize(N);
                break;
            }
            else if (ret <= 0) {
                rxbuf.resize(N);
                klose(ptConnCtx);
                readyState = CLOSED;
                log_error(ret < 0 ? "Connection error!\n" : "Connection closed!\n");
                break;
            }
            else {
                rxbuf.resize(N + ret);
            }
        }
        while (txbuf.size()) {
			if (readyState == CLOSED) {
				txbuf.clear();
				break;
			}
            int ret = kWrite(ptConnCtx, (char*)&txbuf[0], txbuf.size(), 0);
            if (false) { }
            else if (ret < 0 && (socketerrno == SOCKET_EWOULDBLOCK || socketerrno == SOCKET_EAGAIN_EINPROGRESS)) {
                break;
            }
            else if (ret <= 0) {
                klose(ptConnCtx);
                readyState = CLOSED;
                log_error(ret < 0 ? "Connection error!\n" : "Connection closed!\n");
                break;
            }
            else {
                txbuf.erase(txbuf.begin(), txbuf.begin() + ret);
            }
        }
        if (!txbuf.size() && readyState == CLOSING) {
            klose(ptConnCtx);
            readyState = CLOSED;
        }
    }

    // Callable must have signature: void(const std::string & message).
    // Should work with C functions, C++ functors, and C++11 std::function and
    // lambda:
    //template<class Callable>
    //void dispatch(Callable callable)
    virtual void _dispatch(WebSocket::Callback & callable) {
        // TODO: consider acquiring a lock on rxbuf...
        while (true) {
            wsheader_type ws;
            if (rxbuf.size() < 2) { return; /* Need at least 2 */ }
            const uint8_t * data = (uint8_t *) &rxbuf[0]; // peek, but don't consume
            ws.fin = (data[0] & 0x80) == 0x80;
            ws.opcode = (wsheader_type::opcode_type) (data[0] & 0x0f);
            ws.mask = (data[1] & 0x80) == 0x80;
            ws.N0 = (data[1] & 0x7f);
            ws.header_size = 2 + (ws.N0 == 126? 2 : 0) + (ws.N0 == 127? 6 : 0) + (ws.mask? 4 : 0);
            if (rxbuf.size() < ws.header_size) { return; /* Need: ws.header_size - rxbuf.size() */ }
            int i;
            if (ws.N0 < 126) {
                ws.N = ws.N0;
                i = 2;
            }
            else if (ws.N0 == 126) {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 8;
                ws.N |= ((uint64_t) data[3]) << 0;
                i = 4;
            }
            else if (ws.N0 == 127) {
                ws.N = 0;
                ws.N |= ((uint64_t) data[2]) << 56;
                ws.N |= ((uint64_t) data[3]) << 48;
                ws.N |= ((uint64_t) data[4]) << 40;
                ws.N |= ((uint64_t) data[5]) << 32;
                ws.N |= ((uint64_t) data[6]) << 24;
                ws.N |= ((uint64_t) data[7]) << 16;
                ws.N |= ((uint64_t) data[8]) << 8;
                ws.N |= ((uint64_t) data[9]) << 0;
                i = 10;
            }
            if (ws.mask) {
                ws.masking_key[0] = ((uint8_t) data[i+0]) << 0;
                ws.masking_key[1] = ((uint8_t) data[i+1]) << 0;
                ws.masking_key[2] = ((uint8_t) data[i+2]) << 0;
                ws.masking_key[3] = ((uint8_t) data[i+3]) << 0;
            }
            else {
                ws.masking_key[0] = 0;
                ws.masking_key[1] = 0;
                ws.masking_key[2] = 0;
                ws.masking_key[3] = 0;
            }
            if (rxbuf.size() < ws.header_size+ws.N) { return; /* Need: ws.header_size+ws.N - rxbuf.size() */ }

            // We got a whole message, now do something with it:
            if (false) { }
            else if (ws.opcode == wsheader_type::TEXT_FRAME
                || ws.opcode == wsheader_type::CONTINUATION
            ) {
                if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
                receivedData.insert(receivedData.end(), rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);// just feed
                if (ws.fin) {
                    std::string data(receivedData.begin(), receivedData.end());
                    callable((const std::string) data);
                    receivedData.erase(receivedData.begin(), receivedData.end());
                    std::vector<uint8_t> ().swap(receivedData);// free memory
                }
            }
            else if (ws.opcode == wsheader_type::PING) {
                if (ws.mask) { for (size_t i = 0; i != ws.N; ++i) { rxbuf[i+ws.header_size] ^= ws.masking_key[i&0x3]; } }
                std::string data(rxbuf.begin()+ws.header_size, rxbuf.begin()+ws.header_size+(size_t)ws.N);
                sendData(wsheader_type::PONG, data);
            }
            else if (ws.opcode == wsheader_type::PONG) { }
            else if (ws.opcode == wsheader_type::CLOSE) { close(); }
            else { log_error("ERROR: Got unexpected WebSocket message. opcode=%d\n", ws.opcode); close(); }

            rxbuf.erase(rxbuf.begin(), rxbuf.begin() + ws.header_size+(size_t)ws.N);
        }
    }

    void sendPing() {
        sendData(wsheader_type::PING, std::string());
    }

    void send(const std::string& message) {
        sendData(wsheader_type::TEXT_FRAME, message);
    }

    void sendData(wsheader_type::opcode_type type, const std::string& message) {
        // TODO:
        // Masking key should (must) be derived from a high quality random
        // number generator, to mitigate attacks on non-WebSocket friendly
        // middleware:
        const uint8_t masking_key[4] = { 0x12, 0x34, 0x56, 0x78 };
        // TODO: consider acquiring a lock on txbuf...
        if (readyState == CLOSING || readyState == CLOSED) { return; }
        std::vector<uint8_t> header;
        uint64_t message_size = message.size();
        header.assign(2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (useMask ? 4 : 0), 0);
        header[0] = 0x80 | type;
        if (false) { }
        else if (message_size < 126) {
            header[1] = (message_size & 0xff) | (useMask ? 0x80 : 0);
            if (useMask) {
                header[2] = masking_key[0];
                header[3] = masking_key[1];
                header[4] = masking_key[2];
                header[5] = masking_key[3];
            }
        }
        else if (message_size < 65536) {
            header[1] = 126 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 8) & 0xff;
            header[3] = (message_size >> 0) & 0xff;
            if (useMask) {
                header[4] = masking_key[0];
                header[5] = masking_key[1];
                header[6] = masking_key[2];
                header[7] = masking_key[3];
            }
        }
        else { // TODO: run coverage testing here
            header[1] = 127 | (useMask ? 0x80 : 0);
            header[2] = (message_size >> 56) & 0xff;
            header[3] = (message_size >> 48) & 0xff;
            header[4] = (message_size >> 40) & 0xff;
            header[5] = (message_size >> 32) & 0xff;
            header[6] = (message_size >> 24) & 0xff;
            header[7] = (message_size >> 16) & 0xff;
            header[8] = (message_size >>  8) & 0xff;
            header[9] = (message_size >>  0) & 0xff;
            if (useMask) {
                header[10] = masking_key[0];
                header[11] = masking_key[1];
                header[12] = masking_key[2];
                header[13] = masking_key[3];
            }
        }
        // N.B. - txbuf will keep growing until it can be transmitted over the socket:
        txbuf.insert(txbuf.end(), header.begin(), header.end());
        txbuf.insert(txbuf.end(), message.begin(), message.end());
        if (useMask) {
            for (size_t i = 0; i != message.size(); ++i) { *(txbuf.end() - message.size() + i) ^= masking_key[i&0x3]; }
        }
    }

    void close() {
        if(readyState == CLOSING || readyState == CLOSED) { return; }
        readyState = CLOSING;
        uint8_t closeFrame[6] = {0x88, 0x80, 0x00, 0x00, 0x00, 0x00}; // last 4 bytes are a masking key
        std::vector<uint8_t> header(closeFrame, closeFrame+6);
        txbuf.insert(txbuf.end(), header.begin(), header.end());
    }

};

int verify_callback(int res, void*) {
    // Override ssl errors (!!)
    return SSL_SUCCESS;
}

easywsclient::WebSocket::pointer from_url(const std::string& url, bool useMask, const std::string& origin) {
	char sc[128];
    char host[128];
    int port;
    char path[128];
    bool is_ssl = false;
    if (url.size() >= 128) {
      log_error("ERROR: url size limit exceeded: %s\n", url.c_str());
      return NULL;
    }
    if (origin.size() >= 200) {
      log_error("ERROR: origin size limit exceeded: %s\n", origin.c_str());
      return NULL;
    }
    if (false) { }
    else if (sscanf(url.c_str(), "w%[s]://%[^:/]:%d/%s", sc, host, &port, path) == 4) {
    }
    else if (sscanf(url.c_str(), "w%[s]://%[^:/]/%s", sc, host, path) == 3) {
        port = sc[1]=='s'? 443:80;
    }
    else if (sscanf(url.c_str(), "w%[s]://%[^:/]:%d", sc, host, &port) == 3) {
        path[0] = '\0';
    }
    else if (sscanf(url.c_str(), "w%[s]://%[^:/]", sc, host) == 2) {
        port = sc[1]=='s'? 443:80;
        path[0] = '\0';
    }
    else {
        log_error("ERROR: Could not parse WebSocket url: %s\n", url.c_str());
        return NULL;
    }
    if (sc[1]!='\0' && sc[2] != '\0') {
    	log_error("ERROR: Could not parse WebSocket url: %s\n", url.c_str());
        return NULL;
    }
    is_ssl = sc[1]=='s';
    log_error(
    	"easywsclient: connecting: ssl=%s, host=%s port=%d path=/%s\n",
    	is_ssl?"true":"false", host, port, path);

    ConnectionContext* ptConnCtx;
    ptConnCtx = (ConnectionContext*)malloc(sizeof(ConnectionContext));
    ptConnCtx->sslContext = nullptr;
    ptConnCtx->sslHandle = nullptr;
    ptConnCtx->sockfd = hostname_connect(host, port);
    if (ptConnCtx->sockfd == INVALID_SOCKET) {
        log_error("ERROR: Unable to connect to %s:%d\n", host, port);
        free(ptConnCtx);
        return NULL;
    }

    if (is_ssl) {
        int res = 0;
        auto err_out = [res,ptConnCtx]() {
            ERR_print_errors_cb(print_error_callback, 0);
            char err_str[255];
            wolfSSL_ERR_error_string(SSL_get_error(ptConnCtx->sslHandle, 0),err_str);
            log_error("SSL error: %s", err_str);
            return nullptr;
        };
        // ssl handshake
        // Register the error strings for libcrypto & libssl
        SSL_load_error_strings();
        // Register the available ciphers and digests
        SSL_library_init();

        // New context saying we are a client using TLS 1.2
        ptConnCtx->sslContext = SSL_CTX_new(TLSv1_2_client_method());
        if (ptConnCtx->sslContext == NULL)
            return err_out();

        // Create an SSL struct for the connection
        SSL_CTX_set_verify(ptConnCtx->sslContext, SSL_VERIFY_PEER, (VerifyCallback)verify_callback);

        ptConnCtx->sslHandle = SSL_new(ptConnCtx->sslContext);
        if (ptConnCtx->sslHandle == NULL)
            return err_out();

        //SSL_set_verify(ptConnCtx->sslHandle, SSL_VERIFY_NONE, (VerifyCallback)verify_callback);

        if(SSL_CTX_set_session_cache_mode(ptConnCtx->sslContext, SSL_SESS_CACHE_OFF) != SSL_SUCCESS)
            return err_out();

      // Connect the SSL struct to our connection
      if (SSL_set_fd (ptConnCtx->sslHandle, ptConnCtx->sockfd) != SSL_SUCCESS)
          return err_out();

      if(SSL_set_tlsext_host_name(ptConnCtx->sslHandle, host) != SSL_SUCCESS)
          return err_out();

      // Initiate SSL handshake
      if (SSL_connect(ptConnCtx->sslHandle) != SSL_SUCCESS)
          return err_out();
    }
    {
        // XXX: this should be done non-blocking,
        char line[256];
        int status;
        int i;
        snprintf(line, 256, "GET /%s HTTP/1.1\r\n", path); kWrite(ptConnCtx, line, strlen(line), 0);
        if (port == 80) {
            snprintf(line, 256, "Host: %s\r\n", host); kWrite(ptConnCtx, line, strlen(line), 0);
        }
        else {
            snprintf(line, 256, "Host: %s:%d\r\n", host, port); kWrite(ptConnCtx, line, strlen(line), 0);
        }
        snprintf(line, 256, "Upgrade: websocket\r\n"); kWrite(ptConnCtx, line, strlen(line), 0);
        snprintf(line, 256, "Connection: keep-alive, Upgrade\r\n"); kWrite(ptConnCtx, line, strlen(line), 0);
        if (!origin.empty()) {
            snprintf(line, 256, "Origin: %s\r\n", origin.c_str()); kWrite(ptConnCtx, line, strlen(line), 0);
        }
        snprintf(line, 256, "Sec-WebSocket-Key: hLuO7MKwvHBxsv/ureQI9w==\r\n"); kWrite(ptConnCtx, line, strlen(line), 0);
        snprintf(line, 256, "Sec-WebSocket-Version: 13\r\n"); kWrite(ptConnCtx, line, strlen(line), 0);
        snprintf(line, 256, "\r\n"); kWrite(ptConnCtx, line, strlen(line), 0);
        for (i = 0; i < 2 || (i < 255 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) { if (kRead(ptConnCtx, line+i, 1, 0) == 0) { return NULL; } }
        line[i] = 0;
        if (i == 255) { log_error("ERROR: Got invalid status line connecting to: %s\n", url.c_str()); return NULL; }
        if (sscanf(line, "HTTP/1.1 %d", &status) != 1 || status != 101) { log_error("ERROR: Got bad status connecting to %s: %s", url.c_str(), line); return NULL; }
        while (true) {
            for (i = 0; i < 2 || (i < 255 && line[i-2] != '\r' && line[i-1] != '\n'); ++i) { if (kRead(ptConnCtx, line+i, 1, 0) == 0) { return NULL; } }
            if (line[0] == '\r' && line[1] == '\n') { break; }
        }
    }
    int flag = 1;
    setsockopt(ptConnCtx->sockfd, IPPROTO_TCP, TCP_NODELAY, (char*) &flag, sizeof(flag)); // Disable Nagle's algorithm
#ifdef _WIN32
    u_long on = 1;
    ioctlsocket(ptConnCtx->sockfd, FIONBIO, &on);
#else
    fcntl(ptConnCtx->sockfd, F_SETFL, O_NONBLOCK);
#endif
    log_error("Connected to: %s\n", url.c_str());
    return easywsclient::WebSocket::pointer(new _RealWebSocket(ptConnCtx, useMask));
}

} // end of module-only namespace



namespace easywsclient {

WebSocket::pointer WebSocket::create_dummy() {
    static pointer dummy = pointer(new _DummyWebSocket);
    return dummy;
}


WebSocket::pointer WebSocket::from_url(const std::string& url, const std::string& origin) {
    return ::from_url(url, true, origin);
}

WebSocket::pointer WebSocket::from_url_no_mask(const std::string& url, const std::string& origin) {
    return ::from_url(url, false, origin);
}


} // namespace easywsclient
