#ifndef EASYWSCLIENT_SCHANNEL_HPP_
#define EASYWSCLIENT_SCHANNEL_HPP_

#include <string>

#include "easywsclient.hpp"

namespace easywsclient::schannel {
    WebSocket::pointer connect(const std::string& host, int port, bool is_ssl, const std::string& path,
                               const HeaderKeyValuePair& additional_headers, const std::string& origin);
}

#endif /* EASYWSCLIENT_SCHANNEL_HPP_ */
