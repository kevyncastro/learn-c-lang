#include <asm-generic/errno-base.h>
#include <stddef.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "write_all.h"

#define REQ_MAX 8192

struct req {
  int fd;
  char *buf;
  size_t cap;
  size_t *head_len;
};

const char *find_head_end(struct req *request, size_t len) {
  for(size_t i = 0; i + 3 < len; i++) {
    if (request->buf[i] == '\r' && request->buf[i+1] == '\n' && request->buf[i+2] == '\r' && request->buf[i+3] == '\n') {
      return request->buf + i;
    }
  }
  return NULL;
};

const ssize_t read_request_head(struct req *request) {
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
    printf("%.*s", (int)(end - request->buf), request->buf);
    printf("end of headers %s", end);

    if (end) {
      *request->head_len = (size_t)(end - request->buf) + 4;
      return (ssize_t)used;
    }
  }
};

void handle_request(int fd, char *buf, size_t len) {
  char head[612];
  char *body = "WTF";
  int n = snprintf(head, sizeof head,
                   "HTTP/1.1 %d\r\n"
                   "\r\n",
                   200);
  if (n < 0 || (size_t)n >= sizeof head) {
    perror("error");
  }
  write_all(fd, head, (size_t)n);
  write_all(fd, body, strlen(body));
}

void handle_client(int fd) {
  char buf[REQ_MAX];
  size_t head_len;
  struct req request = { fd, buf, sizeof buf, &head_len };
  ssize_t got = read_request_head(&request);
  if (got <= 0) printf("none\n");
  handle_request(fd, buf, sizeof buf);
};
