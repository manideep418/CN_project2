
#include "request_handler.h"
#include "utils.h"
#include "http_utils.h"

pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
std::map<std::string, std::string> url_to_file_cache_map;

extern ParsedArguments parsedArguments;

char rand_char()
{
    const char charset[] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[ rand() % max_index ];
};

std::string random_string( size_t length )
{
    std::string str(length,0);
    std::generate_n( str.begin(), length, rand_char );
    return str;
}

void* handle_client_connection(void* arg)
{
    HostInfo *client_info = (HostInfo *)arg;
    int client_sd = client_info->socket_fd;

    // Receive incoming client's request
    HttpMessage *http_message = read_http_message_from_socket(client_sd);
    if (http_message == NULL) {
        return NULL;
    }
    HttpMessage *http_response_from_target_server;

    std::stringstream msg_stream;
    msg_stream << "Received request from " << client_info->hostname << ":" << client_info->port << ":\n"
		       << http_message->to_log_string();
    log(msg_stream.str());

    // Extract request path on the target server that client wishes to access
    std::string request_path = http_message->get_request_url();

    // Checking the cache first...
    pthread_mutex_lock(&cache_mutex);
    if (url_to_file_cache_map.find(request_path) != url_to_file_cache_map.end()) {
	    std::string cached_filename = url_to_file_cache_map[request_path];
	    pthread_mutex_unlock(&cache_mutex);

	    std::ifstream inFile(cached_filename); //open the cache file

	    std::stringstream strStream;
	    strStream << inFile.rdbuf(); //read the file
	    std::string cached_response = strStream.str(); //str holds the content of the file
	    inFile.close();

	    if (send_to_socket(client_sd, cached_response) < 0) {
		    log("Error while sending target server's reply back to client");
            return NULL;
	    } else {
		    log("Sent cached response back to the client");
	    }

	    // Close the client connection
        close(client_sd);
    } else {
	    pthread_mutex_unlock(&cache_mutex);
    }

    std::vector<std::string> url_parts = split(request_path.substr(1), '/');
    std::string redirect_to = url_parts[0];
    std::string redirect_path = "/" + url_parts[1];

    if (is_host_blocked(redirect_to, parsedArguments.sites_blocklist_filename)) {
	    log("Host " + redirect_to + " is blocked, returning code 401 - Access Denied");
	    http_response_from_target_server = make_http_response("401 Access Denied"); 
    } else {

	    if (redirect_to.empty()) {
		    http_response_from_target_server = make_http_response("404 Not Found"); 
	    }

	    log("Redirecting message to: " + redirect_to);
	    log("Path on target server: " + redirect_path);

	    // Modify the HTTP message before sending it to the target server
	    HttpMessage redirected_message(*http_message);
	    redirected_message.header.path = redirect_path;
	    redirected_message.header.headers["Host"] = redirect_to;

	    log("Redirected request to " + redirect_to + ":\n" + redirected_message.to_log_string());

	    // Send the modified HTTP message to target server
	    int target_sockfd = create_socket_to_server(redirect_to);
	    if (target_sockfd < 0)
	    {
		    // An error occured, TODO: send HTTP 500 back to client
            http_response_from_target_server = make_http_response("404 Not Found");
		    send_to_socket(client_sd, http_response_from_target_server->to_string());
            return NULL;
	    }

	    log("Sending message to target server...");
	    if (send_to_socket(target_sockfd, redirected_message.to_string()) < 0) {
    	    //close(target_sockfd);
		    log("Error while sending modified HTTP message to target server");
		    http_response_from_target_server = make_http_response("404 Not Found");
		    send_to_socket(client_sd, http_response_from_target_server->to_string());
            return NULL;
	    } else {
	        //close(target_sockfd);
	    }

	    // Receive target server's reply
	    http_response_from_target_server = read_http_message_from_socket(target_sockfd);
	    if (http_response_from_target_server == NULL) {
		    http_response_from_target_server = make_http_response("404 Not Found");
		    send_to_socket(client_sd, http_response_from_target_server->to_string());
            return NULL;
	    }
	    
        int redirect_cnt = 0;
	    while (http_response_from_target_server->header.status.find("Moved") != std::string::npos && 
    	    redirect_cnt < 10) {
    	    redirect_cnt++;
    	    
    	    log("Redirecting...");
    	    
    	    std::string loc_redirect = http_response_from_target_server->header.headers["Location"];
    	    redirected_message.header.headers["Referer"] = loc_redirect;
    	    if (loc_redirect.find("http://") == 0) {
    	        loc_redirect = loc_redirect.substr(std::string("http://").size());
    	    }
    	    
    	    log(redirected_message.to_string());
    	    
            redirected_message.header.path = loc_redirect;
    	    
    	    // Try to fetch the resource from redirect URL
	        int target_sockfd = create_socket_to_server(redirect_to);
	        if (target_sockfd < 0)
	        {
		        // An error occured, TODO: send HTTP 500 back to client
                http_response_from_target_server = make_http_response("404 Not Found");
		        send_to_socket(client_sd, http_response_from_target_server->to_string());
                return NULL;
	        }

	        log("Sending message to target server...");
	        if (send_to_socket(target_sockfd, redirected_message.to_string()) < 0) {
        	    //close(target_sockfd);
		        log("Error while sending modified HTTP message to target server");
		        http_response_from_target_server = make_http_response("404 Not Found");
		        send_to_socket(client_sd, http_response_from_target_server->to_string());
                return NULL;
	        } else {
	            //close(target_sockfd);
	        }
	    }

	    log("Received response from target server:\n'" + http_response_from_target_server->to_log_string() + "'");
    }

    // Filter words in the response's body
    if ((http_response_from_target_server->header.headers.find("Content-Type") != http_response_from_target_server->header.headers.end()) && 
	    (http_response_from_target_server->header.headers["Content-Type"] == "text/html" || http_response_from_target_server->header.headers["Content-Type"] == "text/plain")) {
	    http_response_from_target_server->body = filter_words(http_response_from_target_server->body, parsedArguments.filter_words_list_filename);
	    std::stringstream content_length_ss;
	    content_length_ss << http_response_from_target_server->body.size();
	    http_response_from_target_server->header.headers["Content-Length"] = content_length_ss.str();
    }

    // Cache if it's allowed
    if ((http_response_from_target_server->header.headers.find("Cache-Control") == http_response_from_target_server->header.headers.end()) || 
		    ((http_response_from_target_server->header.headers["Cache-Control"] != "max-age=0") && 
		    (http_response_from_target_server->header.headers["Cache-Control"] != "no-cache") && 
		    (http_response_from_target_server->header.headers["Cache-Control"] != "no-store"))) {
	    log("Caching the response");
	    std::string cache_filename = random_string(32);

	    // Create cache directory if not exists
	    struct stat st = {0};
	    if (stat(parsedArguments.cache_directory_path.c_str(), &st) == -1) {
	        mkdir(parsedArguments.cache_directory_path.c_str(), 0700);
	    }
	    // Write target server's response to the newly created file
	    std::ofstream fout;
	    std::string full_path = parsedArguments.cache_directory_path + "/" + cache_filename;
	    fout.open(full_path);
	    fout << http_response_from_target_server->to_string();
	    fout.close();

	    pthread_mutex_lock(&cache_mutex);
	    url_to_file_cache_map[request_path] = full_path;
	    pthread_mutex_unlock(&cache_mutex);
    }

    // Send the target server's reply to the client
    if (send_to_socket(client_sd, http_response_from_target_server->to_string()) < 0) {
	    log("Error while sending target server's reply back to client");
	    return NULL;
    } else {
	    log("Sent response back to the client");
    }
	

	// Close the client connection, clean up the resources
    close(client_sd);
    delete client_info;
    delete http_message;

    return NULL;
}
