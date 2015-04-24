
#pragma once

#include "utils.h"

struct HttpHeader {
    enum Type {
        REQUEST,
        RESPONSE,
        MALFORMED
    } type;
	std::string method;		
	std::string status;
	std::string protocol;
	std::string path;
	std::map<std::string, std::string> headers;
};

struct HttpMessage {
	HttpHeader header;
	std::string body;

	std::string to_string() const;
	std::string to_log_string() const;

	std::string get_request_url() const;
};

HttpMessage* read_http_message_from_socket(int socket_descriptor);

HttpMessage* make_http_response(const std::string &code);
