#include <netdb.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include "open_listener.h"
#include "write_all.h"
#include "handle_client.h"
#include <stdlib.h>

#define PORT "8080"
#define BACKLOG 16

typedef struct {
  char *ptr;
} object;

typedef struct {
  object methods;
  object targets;
  struct Route* (*function)(int status);
} Route;

typedef struct Server {
  int fd;
  void (*Route)();
  void (*start)(char *port, int backlog,struct Server* self);
} Server;

void init_fd(char *port, int backlog, Server *server) {
  int listen_fd = open_listener(port, backlog);
  server->fd = listen_fd;
};

Server* new_server() {
  Server* s = malloc(sizeof(Server));
  s->start = init_fd;
  return s;
};

int main(void) {
  Server* myServer = new_server();
  myServer->start(PORT, BACKLOG, myServer);
  if (myServer->fd < 0) return 1;
  printf("listening on port %s\n", PORT);

  for (;;) {
    int client_fd = accept(myServer->fd, NULL, NULL);
    if (client_fd < 0) {
      perror("accept");
      continue;
    }
    handle_client(client_fd);
    close(client_fd);
  }
}
