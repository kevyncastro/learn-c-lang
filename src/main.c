#include <netdb.h>
#include <stddef.h>
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

typedef struct Route {
  Slice method;
  Slice target;
  struct Route* (*function)(int status);
  struct Route* route;
} Route;

typedef struct Server {
  int fd;
  int (*add_route)(struct Server* self, char *method, char *route, void *function);
  Route *routes;
  size_t RoutesLength;
  size_t RoutesCapacity;
} Server;

int add_route(Server* server, char *method, char *route, void *function) {
  // compare length and capacity
  // if length is equal to capacity then realloc then assign the new Route ti the index length++
  // if length is less than capacity then assign the new Route to the index length++
  if (server->RoutesLength == server->RoutesCapacity) {
    size_t new_capacity = server->RoutesCapacity + 1;
    server->routes = realloc(server->routes, new_capacity * sizeof(Route));
    server->RoutesCapacity = new_capacity;
  };
  size_t position = server->RoutesLength;
  server->routes[server->RoutesLength].target.ptr = route;
  server->routes[server->RoutesLength].method.ptr = method;
  server->routes[server->RoutesLength].function = function;
  server->RoutesLength++;
  return position;
};

// Constructor
Server* new_server(char *port, int backlog) {
  Server* s = calloc(1, sizeof(Server));
  s->routes = (Route*)calloc(1, 1 * sizeof(Route));
  int listen_fd = open_listener(port, backlog);
  s->fd = listen_fd; 
  s->RoutesCapacity = 1;
  s->add_route = add_route;
  return s;
};

void testFn () {}

int main(void) {

  //Server Init
  Server* myServer = new_server(PORT, BACKLOG);

  size_t routePosition;
  // Add routes
  routePosition = myServer->add_route(myServer, "GET", "/test1", testFn );
  printf("Added Route: %s\n", myServer->routes[routePosition].target.ptr);
  routePosition = myServer->add_route(myServer, "GET", "/test2", testFn );
  printf("Added Route: %s\n", myServer->routes[routePosition].target.ptr);

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
