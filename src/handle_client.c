#include <asm-generic/errno-base.h>
#include <stddef.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "write_all.h"

#define REQ_MAX 8192
#define MAX_HEADERS 64

struct req {
  int fd;
  char *buf;
  size_t cap;
  size_t *head_len;
};

struct slice {
    const char *ptr;
    size_t      len;
};

struct header {
    struct slice name;
    struct slice value;
};

struct request {
    struct slice  method;
    struct slice  target;
    struct slice  version;
    struct header headers[MAX_HEADERS];
    size_t        header_count;
};

const char *find_head_end(struct req *request, size_t len) {
  for(size_t i = 0; i + 3 < len; i++) {
    if (request->buf[i] == '\r' && request->buf[i+1] == '\n' && request->buf[i+2] == '\r' && request->buf[i+3] == '\n') {
      return request->buf + i;
    }
  }
  return NULL;
};

const char read_request_head(struct req *request) {
  size_t used = 0;
  for(;;) {
    if (used == request->cap) return -2;
    ssize_t n = read(request->fd, request->buf + used, request->cap - used );
    if (n == 0) return 0;
    else if (n < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    used += n;
    const char *start = request->buf;
    const char *end = find_head_end(request, used);

    if (end) {
      *request->head_len = (size_t)(end - request->buf) + 4;
      return (ssize_t)used;
    }
  }
};

struct parse_request_method_return { struct slice method; char *result; };
struct parse_request_method_return parse_request_method(struct req *request, struct request *client_request) {
  char *result = memchr(request->buf,' ', request->cap);
  char *method = strndup(request->buf, (size_t)(result - request->buf) + 1);
  // client_request->method = (struct slice){method, (size_t)(result - request->buf) + 1};
  const struct parse_request_method_return return_object = { { method, (size_t)(result - request->buf) + 1 }, result };
  return return_object;
}

struct slice parse_request_target(const char *string, struct request *client_request, char *sp1) {
  const char *sp2 = memchr(sp1 + 1, ' ', (size_t)(string - (sp1 + 1)));
  char *target = strndup(sp1 + 1, (size_t)(sp2 - (sp1 + 1)));
  return (struct slice){target, (size_t)(sp2 - target) + 1 };
}

void handle_request(struct req *request) {
  char head[612];
  char *body = "WTF";
  ssize_t got = read_request_head(request);
  if (got <= 0) printf("none\n");
  int n = snprintf(head, sizeof head,
                   "HTTP/1.1 %d\r\n"
                   "\r\n",
                   200);
  if (n < 0 || (size_t)n >= sizeof head) {
    perror("error");
  }
  const char *end = request->buf + got;
  write_all(request->fd, head, (size_t)n);
  write_all(request->fd, body, strlen(body));
  struct request client_request = {};
  struct parse_request_method_return getMethodObject = parse_request_method(request, &client_request);
  client_request.method = getMethodObject.method;
  client_request.target = parse_request_target(end, &client_request, getMethodObject.result);
  printf("Method: %s\n", client_request.method.ptr);
  printf("Target: %s\n", client_request.target.ptr);
}

void handle_client(int fd) {
  char buf[REQ_MAX];
  size_t head_len;
  struct req request = { fd, buf, sizeof buf, &head_len };
  handle_request(&request);
};
