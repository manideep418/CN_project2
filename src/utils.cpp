
#include <iostream>
#include <algorithm>
#include <sstream>
#include <string>
#include <fstream>
#include <ctime>
#include <cstdlib>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <resolv.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <set>

#include "utils.h"

pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

void log(const std::string &msg)
{
    pthread_mutex_lock(&log_mutex);
    std::cout << "SERVER LOG: " << msg << std::endl;
    pthread_mutex_unlock(&log_mutex);
}

void print_error_and_die(const std::string &msg, int exit_status)
{
    pthread_mutex_lock(&log_mutex);
    perror(msg.c_str());
    exit(exit_status);
    pthread_mutex_unlock(&log_mutex);
}

void print_usage_and_die(int exit_status)
{
    const char *USAGE_STRING = "Usage: ./server <PORT NUMBER> <SITES_BLOCKLIST> <WORDS_FILTER> <CACHE_DIRECTORY>";
    std::cerr << USAGE_STRING << std::endl;
    exit(exit_status);
}

ParsedArguments parse_arguments(int argc, char *argv[])
{
    if (argc > 5 || argc < 5) {
        print_usage_and_die();
    }

    ParsedArguments arguments;
    arguments.port = (argc > 1)? atoi(argv[1]) : 8888;
    arguments.sites_blocklist_filename = argv[2];
    arguments.filter_words_list_filename = argv[3];
    arguments.cache_directory_path = argv[4];
    return arguments;
}

int create_listening_socket(struct sockaddr_in *socket_address)
{
    int listening_socket;

    if ((listening_socket = socket(PF_INET, SOCK_STREAM, 0)) < 0 ) 
        print_error_and_die("Error while creating socket");

    int on = 1;
    if (setsockopt(listening_socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
    {
        print_error_and_die("Error while calling setsockopt(...,SO_REUSEADDR,...) for listening socket");
    }

    if (bind(listening_socket, (struct sockaddr*)socket_address, sizeof(*socket_address)) != 0) {
        std::stringstream ss;
        ss << "Error while binding server socket to the port " << socket_address->sin_port;
        print_error_and_die(ss.str());
    }

    if (listen(listening_socket, 32) != 0 )
        print_error_and_die("Error while in listen() call");

    return listening_socket;
}

struct sockaddr_in create_listening_socket_address(const ParsedArguments &arguments)
{
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(arguments.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    return addr;
}

int create_socket_to_server(const std::string &hostport)
{
    std::vector<std::string> parts = split(hostport, ':');
    std::string host = parts[0];
    int port = parts[1].empty()? 80 : std::atoi(parts[1].c_str());
    return create_socket_to_server(host, port);
}

int hostname_to_ip(const std::string &hostname, std::string &ip_out)
{
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
         
    if ((he = gethostbyname(hostname.c_str())) == NULL) {
        log("Error in gethostbyname() while resolving hostname to IP for hostname '" + hostname + "'");
        return 1;
    }
 
    addr_list = (struct in_addr **) he->h_addr_list;
     
    char ip_buf[16];
    for(i = 0; addr_list[i] != NULL; i++) {
        strcpy(ip_buf, inet_ntoa(*addr_list[i]));
        ip_out = std::string(ip_buf);
        return 0;
    }
     
    return 1;
}

int create_socket_to_server(const std::string &host, int port)
{
    int sockfd;
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        log("Couldn't create socket to remote server at " + host);
        return -1;
    } 

    struct sockaddr_in serv_addr; 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port); 

    std::string ip_addr;
    if (hostname_to_ip(host, ip_addr) < 0) {
        return -1;
    }
    log("Resolved hostname " + host + " to ip " + ip_addr);

    if (inet_pton(AF_INET, ip_addr.c_str(), &serv_addr.sin_addr) < 0) {
        log("inet_pton() error occured");
        return -1;
    } 

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       log("connect() error while connecting to remote server " + host);
       return -1;
    } 

    return sockfd;
}

HostInfo* wait_for_client_and_accept(int listening_socket)
{
    HostInfo *client_info = new HostInfo();
  
    struct sockaddr_in client_address;
    socklen_t addr_size = static_cast<socklen_t>(sizeof(client_address));
    client_info->socket_fd = accept(listening_socket, (struct sockaddr*)&client_address, &addr_size);

    char client_host[INET_ADDRSTRLEN];
    if (inet_ntop(AF_INET, &client_address.sin_addr.s_addr, client_host, sizeof(client_host)) != NULL) {
       client_info->hostname = inet_ntoa(client_address.sin_addr);
       client_info->port = ntohs(client_address.sin_port);
       std::stringstream ss;
       ss << "New connection from " << client_info->hostname << ":" << client_info->port;
       log(ss.str());
    } else {
       log("New connection, but unable to get address of the client");
    }

    return client_info;
}

int send_to_socket(int sock, const std::string &data)
{
    const char *buf = data.c_str();
    size_t bytes_left = data.size();
    size_t offset = 0;

    while (bytes_left > 0) {
        int bytes_sent = send(sock, buf + offset, bytes_left, 0);
        if (bytes_sent < 0) {
            return -1;
        }
        offset += bytes_sent;
        bytes_left -= bytes_sent;
    }

    return 0;
}

std::vector<std::string> split(std::string source, char delimiter)
{
    std::vector<std::string> result;
    int colon_pos = source.find(delimiter);
    if (colon_pos != std::string::npos) {
        std::string part_one = source.substr(0, colon_pos);
        std::string part_two = source.substr(colon_pos + 1);
        result.push_back(part_one);
        result.push_back(part_two);
    } else {
        result.push_back(source);
        result.push_back("");
    }
    return result;
}

std::vector<std::string> split_all(std::string source, char delimiter)
{
    std::vector<std::string> result;
    std::string string_left = source;
    while (!string_left.empty()) {
        std::vector<std::string> split_result = split(string_left, delimiter);
        result.push_back(split_result[0]);
        string_left = split_result[1];
    }
    return result;
}

bool is_host_blocked(const std::string &hostname, const std::string &sites_blocked_list_file)
{
    std::ifstream iss;
    iss.open(sites_blocked_list_file);
    std::string str;
    while (std::getline(iss, str)) {
        str = trim(str);
        if (str.empty()) 
            continue;
        if (str == hostname) 
            return true;
    }
    iss.close();

    return false;
}

std::string filter_words(const std::string &str, const std::string &filtered_words_list_file)
{
    static std::vector<std::string> words;

    if (words.empty()) {
        std::ifstream is;
        is.open(filtered_words_list_file);
        std::string str;
        while (std::getline(is, str)) {
            std::string word_lowercase = trim(str);
            std::transform(word_lowercase.begin(), word_lowercase.end(), word_lowercase.begin(), ::tolower);
            words.push_back(word_lowercase);
        }
        is.close();
    }
    
    std::string str_lowercase = std::string(str);
    for (int i = 0; i < str_lowercase.size() - 1; i++) {
        if (str_lowercase[i] == '<') {
            while (i < str_lowercase.size() - 1 && str_lowercase[i] != '>') {
                str_lowercase[i] = ' ';
                i++;
            }
        }
    }

    std::string result = std::string(str);
    std::transform(str_lowercase.begin(), str_lowercase.end(), str_lowercase.begin(), ::tolower);

    for (int i = 0; i < words.size(); i++) {
        std::string &word = words[i];
        std::string::size_type n = 0;
        std::string replacement = "CENSORED";
        while ( ( n = str_lowercase.find( word, n ) ) != std::string::npos )
        {
            str_lowercase.replace( n, word.size(), replacement );
            result.replace( n, word.size(), replacement );
            n += replacement.size();
        }
    }
        
    return result;
}

void print_vector(std::vector<std::string> v)
{
        std::cerr << "VECTOR_BEGIN" << std::endl;
    for (int i = 0; i < v.size(); i++) {
        std::cerr << v[i] << "\n";
    }
            std::cerr << "VECTOR_END" << std::endl;
}
