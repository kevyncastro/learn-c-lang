#include <unistd.h>
#include <stdio.h>

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
	return 0;
};

void handle_client(int fd) {
	char buf[REQ_MAX];
	size_t head_len;
	struct req request = { fd, buf, sizeof buf, &head_len };
	ssize_t got = read_request_head(&request);
	if (got <= 0) printf("none\n");
};
