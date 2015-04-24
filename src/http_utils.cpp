
#include "utils.h"
#include "http_utils.h"

#include "zlib.h"

// The maximum length of HTTP request is 8190, according to Apache docs
const int HTTP_REQUEST_MAX_LENGTH = 8200;

HttpHeader make_http_header_from_string(const std::string &str)
{
	HttpHeader header;
	std::stringstream sstream(str);
	std::string request_line;
	std::getline(sstream, request_line, '\n');
	
	std::cerr << "Request line" << request_line << std::endl;
	
    std::vector<std::string> request_line_parts = split_all(request_line, ' ');
    if (request_line_parts.size() < 3) {
        log("Malformed HTTP header");
        header.type = HttpHeader::MALFORMED;
        return header;
    }
    if (request_line_parts[0].find("HTTP") != std::string::npos) {
        header.type = HttpHeader::RESPONSE;
        header.protocol = request_line_parts[0];
        header.status = "";
        for (int i = 1; i < request_line_parts.size(); i++) {
            header.status += request_line_parts[i];
            if (i < request_line_parts.size() - 1) {
                header.status += " ";
            }
        }
    } else {
        header.type = HttpHeader::REQUEST;
        header.method = request_line_parts[0];
        header.path = request_line_parts[1];        
        header.protocol = request_line_parts[2];         
    }

	std::string line;
	while (std::getline(sstream, line, '\n')) {
		std::vector<std::string> header_parts = split(line, ':');
		if (header_parts[1].empty()) {
			log("Malformed HTTP header line: " + line);
		} else {
			header.headers[header_parts[0]] = trim(header_parts[1]);
		}
	}

	return header;
}

HttpMessage* make_http_response(const std::string &code)
{
    HttpMessage *msg = new HttpMessage();
    msg->body = "<h1>Status: " + code + "</h1>";
    std::stringstream stream;
    stream << msg->body.size();
    msg->header.headers["Content-Length"] = stream.str();
    msg->header.protocol = "HTTP/1.1";
    msg->header.status = code;
    return msg;
}

#define MOD_GZIP_ZLIB_CFACTOR    9
#define MOD_GZIP_ZLIB_BSIZE      8096
#define MOD_GZIP_ZLIB_WINDOWSIZE 15

std::string decompress_deflate(const std::string& str)
{
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit(&zs) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer,
                             zs.total_out - outstring.size());
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") "
            << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

std::string decompress_gzip(const std::string& str)
{
    z_stream zs;                        // z_stream is zlib's control structure
    memset(&zs, 0, sizeof(zs));

    if (inflateInit2(&zs, MAX_WBITS + 32) != Z_OK)
        throw(std::runtime_error("inflateInit failed while decompressing."));

    zs.next_in = (Bytef*)str.data();
    zs.avail_in = str.size();

    int ret;
    char outbuffer[32768];
    std::string outstring;

    // get the decompressed bytes blockwise using repeated calls to inflate
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);

        ret = inflate(&zs, 0);

        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer,
                             zs.total_out - outstring.size());
        }

    } while (ret == Z_OK);

    inflateEnd(&zs);

    if (ret != Z_STREAM_END) {          // an error occurred that was not EOF
        std::ostringstream oss;
        oss << "Exception during zlib decompression: (" << ret << ") "
            << zs.msg;
        throw(std::runtime_error(oss.str()));
    }

    return outstring;
}

HttpMessage* read_http_message_from_socket(int sd)
{
	char buffer[HTTP_REQUEST_MAX_LENGTH];
    int bytes_read = 0;

    std::string header_string;
    std::string body_string;

    // Reading HTTP header request from the client
    do
    {
        int bytes_read_this_iteration = recv(sd, buffer + bytes_read, sizeof(buffer), 0);
        if (bytes_read_this_iteration < 0) {
        	log("Error in recv() while reading data from the client's socket");
        	return nullptr;
        }
        if (bytes_read_this_iteration == 0)
        	break;
        bytes_read += bytes_read_this_iteration;
	    buffer[bytes_read] = '\0';

        // Check if the end of HTTP header was encountered
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end != NULL) {

       		// Reached end of headers
			*headers_end = '\0';
			header_string = std::string(buffer);

			buffer[bytes_read] = '\0';
			if ((headers_end + 4 - buffer) < bytes_read) {
				// We've read a part of message's body - append it to body_string
				std::string t;
				t.resize(bytes_read - (headers_end + 4 - buffer));
				std::copy(buffer + (headers_end + 4 - buffer), buffer + bytes_read, t.begin());
				body_string += t;
			}

			break;
        }

    }
    while (true);

    HttpHeader http_header = make_http_header_from_string(header_string);
    if (http_header.type == HttpHeader::MALFORMED)
        return NULL;
    HttpMessage *result = new HttpMessage();
    result->header = http_header;
    
    log("END OF HTTP HEADERS");

    // If there's Content-Length or Content-Encoding header, read the body
    if ((http_header.headers.find("Content-Length") != http_header.headers.end()) || 
    	(http_header.headers.find("Content-Encoding") != http_header.headers.end()) || 
    	(http_header.headers.find("Transfer-Encoding") != http_header.headers.end())) {

    	if ((http_header.headers.find("Content-Length") != http_header.headers.end()) && (http_header.headers.find("Content-Encoding") == http_header.headers.end()) || 
    		((http_header.headers.find("Content-Encoding") != http_header.headers.end()) &&             (http_header.headers["Content-Encoding"] == "identity"))) {
    		// Case 1: body is not compressed

	    	int content_length = atoi(http_header.headers["Content-Length"].c_str());
	    	int bytes_left = content_length - body_string.size();

	    	if (bytes_left > 0) {
		    	do {
					int bytes_read_this_iteration = recv(sd, buffer, bytes_left, 0);
					
			        if (bytes_read_this_iteration < 0) {
			        	log("Error in recv() while reading data from the client's socket");
			        	return nullptr;
			        }

			        bytes_left -= bytes_read_this_iteration;

			        if (bytes_read_this_iteration == 0) {
			        	// No more data from the client
			        	break;
			        } else {
			        	std::string t;
			        	t.resize(bytes_read_this_iteration);
			        	std::copy(buffer, buffer + bytes_read_this_iteration, t.begin());
			        	body_string += t;
			        }

		    	} while (true);
	    	}
    	} else {
    		// Case 2: body is compressed - just read body until no more data in the socket

    		do {
				int bytes_read_this_iteration = recv(sd, buffer, sizeof(buffer), 0);
				
		        if (bytes_read_this_iteration < 0) {
		        	log("Error in recv() while reading data from the client's socket");
		        	return nullptr;
		        }

		        if (bytes_read_this_iteration == 0) {
		        	break;
		        } else {
		        	std::string t;
		        	t.resize(bytes_read_this_iteration);
		        	std::copy(buffer, buffer + bytes_read_this_iteration, t.begin());
		        	body_string += t;
		        }

		    } while (true);

		    log("Target server's reply is compressed - starting decompresison");
		    try {
		    
		    	std::string encoding = trim(http_header.headers["Content-Encoding"]);
		    	if (encoding == "gzip") {
					body_string = decompress_gzip(body_string);
				} else if (encoding == "deflate") {
					body_string = decompress_deflate(body_string);
				}
				result->header.headers["Content-Encoding"] = "identity";
				std::stringstream content_length_stream;
				content_length_stream << body_string.size();
				result->header.headers["Content-Length"] = content_length_stream.str();
			} catch (std::runtime_error e) {
				log("Error while uncompressing target server's response: " + std::string(e.what()));
				return NULL;
			}
    	}
    } else {
    	body_string = "";
    }

	if (result->header.headers.find("Referer") != result->header.headers.end()) {
		std::string referer_string = result->header.headers["Referer"];
        std::vector<std::string> referer_string_parts = split_all(referer_string, '/');
        if (referer_string_parts.size() > 3) {
            std::string referer_base = referer_string_parts[3];
            if (result->header.path.find("/" + referer_base) != 0) {
                log("Using Referer to rewrite path string in HTTP response to target server");
                if (result->header.path == "/") {
                    result->header.path = "/" + referer_base;
                } else {
                    result->header.path = "/" + referer_base + "/" + result->header.path;
                }
            }
        }
	}

	result->body = body_string;
    return result;
}

std::string HttpMessage::get_request_url() const 
{
	return this->header.path;
}

std::string HttpMessage::to_log_string() const
{
	std::stringstream sstream;
	if (this->header.type == HttpHeader::REQUEST) {
    	sstream << "Status/request line:\n\t" << this->header.method << " " << this->header.path << 
    	    " " << this->header.protocol;
	} else {
	    sstream << "Status/request line:\n\t" << this->header.protocol << " " << this->header.status;
	}
	sstream << "\n" << "Headers:\n";
	for (std::map<std::string, std::string>::const_iterator iterator = this->header.headers.begin(); 
			iterator != this->header.headers.end(); iterator++) {
    	sstream << "\t" << iterator->first << ": " << iterator->second << "\n";
	}
	sstream << "Body: " << this->body.size() << " bytes long, omitted in logs";
	return sstream.str();
}

std::string HttpMessage::to_string() const
{
	std::stringstream sstream;
	if (this->header.type == HttpHeader::REQUEST) {
    	sstream << this->header.method << " " << this->header.path << 
    	    " " << this->header.protocol;
	} else {
	    sstream << this->header.protocol << " " << this->header.status;
	}
	sstream << "\r\n";
	for (std::map<std::string, std::string>::const_iterator iterator = this->header.headers.begin();
	 		iterator != this->header.headers.end(); iterator++) {
    	sstream << iterator->first << ": " << trim(iterator->second) << "\r\n";
	}
	sstream << "\r\n";
	if (!this->body.empty()) {
		sstream << this->body;
	}
	return sstream.str();
}
