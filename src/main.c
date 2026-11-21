#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include "open_listener.h"
#include "write_all.h"
#include "handle_client.h"

#define PORT "8080"
#define BACKLOG 16

typedef struct {
  char *ptr;
} object;

typedef struct {
  object methods;
  object targets;
} Routes;

typedef struct {
  Routes routes;
  int fd;
  // function pointer for adding routes
} Server;

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
    handle_client(client_fd);
    close(client_fd);
  }
}
