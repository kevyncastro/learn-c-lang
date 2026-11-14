#include <asm-generic/errno-base.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define REQ_MAX 8192

struct req {
	int fd;
	char *buf;
	size_t cap;
	size_t *head_len;
};

const char *find_head_end(struct req *request) {

};

const ssize_t read_request_head(struct req *request) {
	size_t used = 0;
	for(;;) {
		if (used == request -> cap) return -2;
		ssize_t n = read(request->fd, request->buf + used, request->cap - used );
		if (n == 0) return 0;
		else if (n < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		used += n;

	}
};

void handle_client(int fd) {
	char buf[REQ_MAX];
	size_t head_len;
	struct req request = { fd, buf, sizeof buf, &head_len };
	ssize_t got = read_request_head(&request);
	if (got <= 0) printf("none\n");
    	printf("---- request ----\n%.*s", (int)head_len, buf);
};
