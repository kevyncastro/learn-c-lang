#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

int write_all(int fd, const char *data, size_t len) {
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

