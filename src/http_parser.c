#include "http_parser.h"
#include <string.h>
#include <stdio.h>

int parse_http_request(const char *raw_request, HttpRequest *out_request) {
    if (!raw_request || !out_request) return -1;

    char buffer[2048];
    strncpy(buffer, raw_request, sizeof(buffer) - 1);

    char *line = strtok(buffer, "\r\n");
    if (!line) return -1;

    char method[16], path[256];
    if (sscanf(line, "%15s %255s", method, path) < 2) return -1;

    if (strcmp(method, "GET") == 0) out_request->method = METHOD_GET;
    else if (strcmp(method, "POST") == 0) out_request->method = METHOD_POST;
    else out_request->method = METHOD_UNKNOWN;

    strncpy(out_request->path, path, MAX_PATH_SIZE - 1);
    return 0;
}

const char* method_to_str(HttpMethod method) {
    switch (method) {
        case METHOD_GET: return "GET";
        case METHOD_POST: return "POST";
        default: return "UNKNOWN";
    }
}