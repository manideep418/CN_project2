#pragma once

/**
 * This function is called in separate thread for each client request and
 * processes the request.
 */
void* handle_client_connection(void* arg);

