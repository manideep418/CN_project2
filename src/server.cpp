
#include "request_handler.h"
#include "utils.h"

ParsedArguments parsedArguments;

int main(int argc, char* argv[])
{
    parsedArguments = parse_arguments(argc, argv);
    log("Launching server...");

    struct sockaddr_in listening_socket_address = create_listening_socket_address(parsedArguments);
    int listening_socket = create_listening_socket(&listening_socket_address);

    log("Started listening to client connecitons " + std::to_string(parsedArguments.port));

    // Entering the main loop, waiting for clients to connect
    while (true)
    {
        HostInfo* client_info = wait_for_client_and_accept(listening_socket);

        pthread_t client_thread;
        if ( pthread_create(&client_thread, NULL, handle_client_connection, client_info) != 0 ) {
            print_error_and_die("Error while spawning thread for processing client's request");
        } else {
            pthread_detach(client_thread);
        }
    }
}
