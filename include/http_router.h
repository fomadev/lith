#ifndef HTTP_ROUTER_H
#define HTTP_ROUTER_H

#include "server.h"
#include "http_parser.h"

void handle_http_route(ExpandedClientContext *ectx, HttpRequest *req, char *full_buffer, int total_received);
void send_http_error(socket_t client_socket, int status_code, const char *status_text, const char *description);

#endif // HTTP_ROUTER_H