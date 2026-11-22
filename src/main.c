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
} Slice;

typedef struct {
  Slice method;
  Slice target;
  struct Route* (*function)(int status);
  struct Route* route;
} Route;

typedef struct Server {
  int fd;
  void (*start)(char *port, int backlog,struct Server* self);
  void (*add_route)(struct Server* self, char *method, char *route, void *function);
  Route routes[5];
} Server;

void init_fd(char *port, int backlog, Server *server) {
  int listen_fd = open_listener(port, backlog);
  server->fd = listen_fd;
};

void add_route(Server* server, char *method, char *route, void *function) {
  Route newRoute = { .method = { method }, .target = route, .function = function };
  server->routes[0] = newRoute;
};

// Constructor
Server* new_server() {
  Server* s = malloc(sizeof(Server));
  s->start = init_fd;
  s->add_route = add_route;
  return s;
};

void testFn () {}

int main(void) {

  //Server Init
  Server* myServer = new_server();
  myServer->start(PORT, BACKLOG, myServer);


  // Add routes
  myServer->add_route(myServer, "GET", "/test", testFn );

  // Checking
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
