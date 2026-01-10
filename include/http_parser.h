#ifndef HTTP_PARSER_H
#define HTTP_PARSER_H

#define MAX_PATH_SIZE 256
#define MAX_BODY_SIZE 1024

typedef enum {
    METHOD_GET,
    METHOD_POST,
    METHOD_UNKNOWN
} HttpMethod;

typedef struct {
    HttpMethod method;
    char path[MAX_PATH_SIZE];
    char body[MAX_BODY_SIZE];
    int content_length;
} HttpRequest;

int parse_http_request(const char *raw_request, HttpRequest *out_request);
const char* method_to_str(HttpMethod method);

#endif