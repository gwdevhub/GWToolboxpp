#pragma once

#include <stdint.h>
#include <memory>
#include <string>

class SchannelTlsStream {
public:
    SchannelTlsStream();
    SchannelTlsStream(const SchannelTlsStream&) = delete;
    SchannelTlsStream& operator=(const SchannelTlsStream&) = delete;
    ~SchannelTlsStream();

    bool Connect(const std::string& host, uint16_t port, bool use_tls, int connect_timeout_ms, int rw_timeout_ms);
    bool Send(const char* data, size_t len);
    int Receive(char* buf, size_t len);
    void Shutdown();
    uint32_t LastError() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
