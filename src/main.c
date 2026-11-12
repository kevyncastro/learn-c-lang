#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT "8080"
#define BACKLOG 16

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
  printf("test %d\n", fd);
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
