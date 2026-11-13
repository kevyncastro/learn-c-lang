#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT "8080"
#define BACKLOG 16

static int write_all(int fd, const char *data, size_t len) {
  while (len > 0) {
    ssize_t n = write(fd, data, len);
      if (n < 0) {
        if (errno == EINTR) continue;
        return -1;
      }
    data += n;
    len -= (size_t)n;
  }
  return 0;
}


static int open_listener(const char *port) {
  struct addrinfo hints, *res, *ai;
  memset(&hints, 0, sizeof hints);
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  
  int rc = getaddrinfo(NULL, port, &hints, &res);
  if (rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
    return -1;
  }

  int fd = -1;
  for (ai = res; ai != NULL; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd<0) continue;
    int yes = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    close(fd);
    fd = -1;
  };
  freeaddrinfo(res);

  if (fd < 0) {
    perror("bind");
    return -1;
  }
  if (listen(fd, BACKLOG) < 0) {
    perror("listen");
    close(fd);
    return -1;
  }
  return fd;
}

static void echo_client(int fd) {
    char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        if (n == 0) return;
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("read");
            return;
        }
        if (write_all(fd, buf, (size_t)n) < 0) {
            perror("write");
            return;
        }
    }
}

int main(void) {
  int listen_fd = open_listener(PORT);
  if (listen_fd < 0) return 1;
  printf("listening on port %s\n", PORT);

  for (;;) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
       perror("accept");
       continue;
    }
    printf("client connected\n");
    echo_client(client_fd);
    close(client_fd);
    printf("client left\n");
  }
}
