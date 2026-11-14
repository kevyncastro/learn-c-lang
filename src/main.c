#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "open_listener.h"
#include "write_all.h"
#include "handle_client.h"

#define PORT "8080"
#define BACKLOG 16

static void echo_client(int fd) {
    char buf[4096];
    for (;;) {
        ssize_t n = read(fd, buf, sizeof buf);
        printf("%ld\n", n);
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
  int listen_fd = open_listener(PORT, BACKLOG);
  if (listen_fd < 0) return 1;
  printf("listening on port %s\n", PORT);

  for (;;) { 
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd < 0) {
       perror("accept");
       continue;
    }
    printf("client connected\n");
    handle_client(client_fd);
    close(client_fd);
    printf("client left\n");
  }
}
