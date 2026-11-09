#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netdb.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#define PORT    "8080"
#define BACKLOG 16
#define REQ_MAX 8192
#define MAX_HEADERS 64

static int write_all(int fd, const char *data, size_t len) {
    while (len > 0) {
        ssize_t n = write(fd, data, len);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        data += n;
        len  -= (size_t)n;
    }
    return 0;
}

static int open_listener(const char *port) {
    struct addrinfo hints = {0}, *res, *ai;
    hints.ai_family   = AF_UNSPEC; hints.ai_socktype = SOCK_STREAM; hints.ai_flags    = AI_PASSIVE; int rc = getaddrinfo(NULL, port, &hints, &res);
    if (rc != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
        return -1;
    }

    int fd = -1;
    for (ai = res; ai != NULL; ai = ai->ai_next) {
        fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) continue;
        int yes = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
        if (bind(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
        close(fd);
        fd = -1;
    }
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

static const char *find_head_end(const char *buf, size_t len) {
    for (size_t i = 0; i + 3 < len; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' &&
            buf[i + 2] == '\r' && buf[i + 3] == '\n')
            return buf + i;
    }
    return NULL;
}

static ssize_t read_request_head(int fd, char *buf, size_t cap,
                                 size_t *head_len) {
    size_t used = 0;
    for (;;) {
        if (used == cap) return -2;
        ssize_t n = read(fd, buf + used, cap - used);
        if (n == 0) return 0;
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        used += (size_t)n;
        const char *end = find_head_end(buf, used);
        if (end) {
            *head_len = (size_t)(end - buf) + 4;
            return (ssize_t)used;
        }
    }
}

struct slice { const char *ptr; size_t len; };
struct header { struct slice name; struct slice value; };
struct request {
    struct slice method, target, version;
    struct header headers[MAX_HEADERS];
    size_t header_count;
};

static int slice_eq(struct slice s, const char *lit) {
    size_t n = strlen(lit);
    return s.len == n && memcmp(s.ptr, lit, n) == 0;
}

static int parse_request_line(const char *p, const char *end,
                              struct request *req) {
    const char *sp1 = memchr(p, ' ', (size_t)(end - p));
    if (!sp1) return -1;
    const char *sp2 = memchr(sp1 + 1, ' ', (size_t)(end - (sp1 + 1)));
    if (!sp2) return -1;

    req->method  = (struct slice){ p, (size_t)(sp1 - p) };
    req->target  = (struct slice){ sp1 + 1, (size_t)(sp2 - (sp1 + 1)) };
    req->version = (struct slice){ sp2 + 1, (size_t)(end - (sp2 + 1)) };

    if (req->method.len == 0 || req->target.len == 0) return -1;
    if (req->target.ptr[0] != '/') return -1;
    if (memchr(req->version.ptr, ' ', req->version.len)) return -1;
    return 0;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static ssize_t percent_decode(const char *in, size_t len,
                              char *out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < len; i++) {
        char ch = in[i];
        if (ch == '%') {
            if (i + 2 >= len) return -1;
            int hi = hex_value(in[i + 1]);
            int lo = hex_value(in[i + 2]);
            if (hi < 0 || lo < 0) return -1;
            ch = (char)(hi * 16 + lo);
            if (ch == '\0') return -1;
            i += 2;
        }
        if (o + 1 >= cap) return -1;
        out[o++] = ch;
    }
    out[o] = '\0';
    return (ssize_t)o;
}

static int resolve_path(const char *root, size_t root_len,
                        const char *decoded, char *out) {
    char joined[PATH_MAX];
    int n = snprintf(joined, sizeof joined, "%s%s", root, decoded);
    if (n < 0 || (size_t)n >= sizeof joined) return -1;
    if (!realpath(joined, out)) return -1;
    if (strncmp(out, root, root_len) != 0) return -2;
    if (out[root_len] != '/' && out[root_len] != '\0') return -2;
    return 0;
}

struct client_info { int fd; char ip[INET6_ADDRSTRLEN]; };
static void handle_client(int fd, const char *ip) { (void)fd; (void)ip; }
static void *client_thread(void *arg) {
    struct client_info info = *(struct client_info *)arg;
    free(arg);
    handle_client(info.fd, info.ip);
    close(info.fd);
    return NULL;
}

struct conn { int fd; char buf[REQ_MAX]; size_t used; };
static void conn_consume(struct conn *c, size_t n) {
    memmove(c->buf, c->buf + n, c->used - n);
    c->used -= n;
}

static int parse_content_length(struct slice v, size_t *out) {
    if (v.len == 0 || v.len > 18) return -1;
    size_t n = 0;
    for (size_t i = 0; i < v.len; i++) {
        if (v.ptr[i] < '0' || v.ptr[i] > '9') return -1;
        n = n * 10 + (size_t)(v.ptr[i] - '0');
    }
    *out = n;
    return 0;
}

struct buf { char *data; size_t len, cap; };
enum conn_state { CONN_READING, CONN_WRITING };
struct econn { int fd; enum conn_state state; struct buf out; };

static int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int buf_append(struct buf *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        size_t cap = b->cap ? b->cap : 1024;
        while (cap < b->len + n) cap *= 2;
        char *p = realloc(b->data, cap);
        if (!p) return -1;
        b->data = p;
        b->cap  = cap;
    }
    memcpy(b->data + b->len, src, n);
    b->len += n;
    return 0;
}

static void accept_all(int ep, int listen_fd) {
    for (;;) {
        int fd = accept(listen_fd, NULL, NULL);
        if (fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) return;
            if (errno == EINTR) continue;
            perror("accept");
            return;
        }
        struct econn *c = calloc(1, sizeof *c);
        if (!c || set_nonblocking(fd) < 0) {
            free(c);
            close(fd);
            continue;
        }
        c->fd    = fd;
        c->state = CONN_READING;
        struct epoll_event ev = { .events = EPOLLIN, .data.ptr = c };
        if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ev) < 0) {
            close(fd);
            free(c);
        }
    }
}

#define CHECK(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); fails++; } } while (0)

int main(void) {
    int fails = 0;
    (void)write_all; (void)open_listener; (void)read_request_head;
    (void)client_thread; (void)accept_all;

    const char *req = "GET /a%20b HTTP/1.1\r\nHost: x\r\n\r\nGET";
    size_t len = strlen(req);
    const char *e = find_head_end(req, len);
    CHECK(e && (size_t)(e - req) + 4 == len - 3);

    struct request r = {0};
    const char *line = "GET /a%20b HTTP/1.1";
    CHECK(parse_request_line(line, line + strlen(line), &r) == 0);
    CHECK(slice_eq(r.method, "GET") && slice_eq(r.target, "/a%20b") && slice_eq(r.version, "HTTP/1.1"));
    const char *bad = "GET /x";
    CHECK(parse_request_line(bad, bad + strlen(bad), &r) == -1);

    char out[64];
    CHECK(percent_decode("/a%20b", 6, out, sizeof out) == 4 && strcmp(out, "/a b") == 0);
    CHECK(percent_decode("/%zz", 4, out, sizeof out) == -1);
    CHECK(percent_decode("/%00", 4, out, sizeof out) == -1);
    CHECK(percent_decode("/%2", 3, out, sizeof out) == -1);
    CHECK(percent_decode("/%2e", 4, out, sizeof out) == 2 && strcmp(out, "/.") == 0);

    char root[PATH_MAX], res[PATH_MAX];
    CHECK(realpath("/usr/share", root) != NULL);
    size_t rl = strlen(root);
    CHECK(resolve_path(root, rl, "/../../etc/passwd", res) == -2);
    CHECK(resolve_path(root, rl, "/nonexistent-xyz", res) == -1);
    CHECK(resolve_path(root, rl, "/", res) == 0);

    size_t cl;
    CHECK(parse_content_length((struct slice){"123", 3}, &cl) == 0 && cl == 123);
    CHECK(parse_content_length((struct slice){"-5", 2}, &cl) == -1);
    CHECK(parse_content_length((struct slice){"", 0}, &cl) == -1);

    struct conn c = {0};
    memcpy(c.buf, "abcdef", 6); c.used = 6;
    conn_consume(&c, 2);
    CHECK(c.used == 4 && memcmp(c.buf, "cdef", 4) == 0);

    struct buf b = {0};
    for (int i = 0; i < 300; i++) buf_append(&b, "hello", 5);
    CHECK(b.len == 1500 && memcmp(b.data + 1495, "hello", 5) == 0);
    free(b.data);

    printf(fails ? "%d failures\n" : "all checks passed\n", fails);
    return fails != 0;
}

