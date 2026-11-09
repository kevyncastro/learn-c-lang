# HTTP Server Workbook

The companion to [http-server-concepts.md](http-server-concepts.md). The concepts guide tells you *what* to learn. This workbook walks you through each milestone with a plan, function signatures, skeleton code with `TODO`s, tests, and hints that reveal a little more each time you open one.

It still does not hand you a finished server. You write the bodies of the functions.

## How to use the hints

Every milestone has three levels of hint, hidden behind a click:
- **Hint 1** is a nudge: which function or idea to look at.
- **Hint 2** is the shape of the answer: pseudocode or the key lines.
- **Hint 3** is a working version of the hardest part.

Try for 15 to 20 minutes before opening a hint, and open them in order. If you use Hint 3, close it, wait a bit, and rewrite the code from memory. That is where the learning happens.

## Contents

- [Setup](#setup)
- [Milestone 1: Echo server](#milestone-1-echo-server)
- [Milestone 2: Hard-coded HTTP response](#milestone-2-hard-coded-http-response)
- [Milestone 3: Parsing requests](#milestone-3-parsing-requests)
- [Milestone 4: Serving static files](#milestone-4-serving-static-files)
- [Milestone 5: Hardening](#milestone-5-hardening)
- [Milestone 6: Many clients with threads](#milestone-6-many-clients-with-threads)
- [Milestone 7: Keep-alive](#milestone-7-keep-alive)
- [Milestone 8: Event loop with epoll](#milestone-8-event-loop-with-epoll)
- [Milestone 9: Make it yours](#milestone-9-make-it-yours)
- [Troubleshooting](#troubleshooting)

---

## Setup

### Project layout

Start with everything in one file, `src/server.c`. Split it up at milestone 4, when it gets long:

```
http-server/
├── Makefile
├── public/          your test website
└── src/
    ├── main.c       startup and the accept loop
    ├── net.c/.h     open_listener, write_all
    ├── http.c/.h    parsing and responses
    └── files.c/.h   paths, MIME types, sending files
```

When you split, functions used by other files lose their `static` keyword and get a prototype in the matching `.h` file.

### Makefile

Paste this into `Makefile`. The indented lines must start with a **tab**, not spaces.

```make
CC      = gcc
CFLAGS  = -std=c11 -D_GNU_SOURCE -g -O1 -Wall -Wextra -pthread \
          -fsanitize=address,undefined
LDFLAGS = -pthread -fsanitize=address,undefined
SRC     = $(wildcard src/*.c)
OBJ     = $( kevyn   http-server-c   main ≡  ?2 ~2 -2    make
gcc -std=c11 -D_GNU_SOURCE -g -O1 -Wall -Wextra -pthread -fsanitize=address,undefined   -c -o src/files.o src/files.c
cc1: fatal error: src/files.c: No such file or directory
compilation terminated.
make: *** [<builtin>: src/files.o] Error 1
 kevyn   http-server-c   main ≡  ?2 ~2 -2    SRC:.c=.o)

server: $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

clean:
	rm -f $(OBJ) server
```

Build with `make`, run with `./server`.

**Why `-D_GNU_SOURCE`?** With `-std=c11`, the compiler hides everything that is not plain C, including POSIX functions such as `getaddrinfo`, `realpath` and `strncasecmp`. This define makes them visible again.

### Two terminals

Keep one terminal running the server and another for testing with `curl` and `nc`. When the server prints something, you see it straight away.

### One trick you will use constantly

Parts of a request are not null-terminated strings (you will see why in milestone 3). To print `len` bytes starting at `ptr`, use the `.*` precision:

```c
printf("%.*s\n", (int)len, ptr);
```

---

## Milestone 1: Echo server

**Goal:** a server that sends back whatever a client types.

**Concepts:** [Sockets](http-server-concepts.md#sockets), [TCP is a byte stream](http-server-concepts.md#tcp-is-a-byte-stream).

### The plan

1. `open_listener` creates a socket, binds it to port 8080 and starts listening.
2. `main` loops forever: `accept` a client, echo to it, `close` it.
3. `echo_client` reads chunks and writes each one back with `write_all`.

### Skeleton

```c
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT    "8080"
#define BACKLOG 16

/* Writes all len bytes, retrying after partial writes.
   Returns 0 on success, -1 on error. */
static int write_all(int fd, const char *data, size_t len) {
    /* TODO: loop until len reaches 0. Retry on EINTR. */
}

/* Creates a listening TCP socket on the given port.
   Returns the fd, or -1 on failure. */
static int open_listener(const char *port) {
    /* TODO:
       1. Fill a struct addrinfo "hints" (TCP, any family, passive).
       2. Call getaddrinfo(NULL, port, ...).
       3. For each result: socket(), setsockopt(SO_REUSEADDR), bind().
          Stop at the first one that binds.
       4. freeaddrinfo().
       5. listen().
    */
}

/* Reads from fd and writes the same bytes back until the client hangs up. */
static void echo_client(int fd) {
    /* TODO: read into a buffer; 0 means the client closed; < 0 is an error
       (but retry on EINTR); otherwise write_all what you read. */
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
```

### Test it

```bash
./server
```

In the other terminal:

```bash
nc localhost 8080
```

Type a line and press Enter: it should come back. Press Ctrl+C in `nc`. The server should print "client left" and keep running. Connect again to prove it.

### Hints

<details>
<summary>Hint 1</summary>

For `write_all`, remember that `write` returns how many bytes it actually wrote, which can be fewer than you asked for. Move a pointer forward by that amount.

For `open_listener`, read `man 3 getaddrinfo`. Its EXAMPLES section has a server.

</details>

<details>
<summary>Hint 2</summary>

`open_listener` in pseudocode:

```
hints = all zeros
hints.ai_family = AF_UNSPEC, ai_socktype = SOCK_STREAM, ai_flags = AI_PASSIVE
getaddrinfo(NULL, port, &hints, &res)  -> on error print gai_strerror and return -1
fd = -1
for ai in res list:
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol)
    if fd < 0: continue
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes)
    if bind(fd, ai->ai_addr, ai->ai_addrlen) == 0: break
    close(fd); fd = -1
freeaddrinfo(res)
if fd < 0: return -1
listen(fd, BACKLOG)
return fd
```

</details>

<details>
<summary>Hint 3</summary>

```c
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
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags    = AI_PASSIVE;

    int rc = getaddrinfo(NULL, port, &hints, &res);
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
```

`echo_client` is yours: it is the same read loop as the file-copy exercises in the study guide, with a socket instead of a file.

</details>

### Experiments

- Connect with two `nc` windows at once. The second one gets no echo until the first quits. Why? (Milestone 6 fixes this.)
- Remove the `SO_REUSEADDR` line, then stop and restart the server quickly. What error do you get?

---

## Milestone 2: Hard-coded HTTP response

**Goal:** `curl` and a browser show "Hello, world!", whatever path they ask for.

**Concepts:** [The HTTP message format](http-server-concepts.md#the-http-message-format), framing.

### The plan

1. Replace `echo_client` with `handle_client`.
2. `read_request_head` keeps reading until the buffer contains `\r\n\r\n`, the end of the headers.
3. Print the request so you can see what clients send.
4. Write a fixed response with a correct `Content-Length`, then close.

### Skeleton

```c
#define REQ_MAX 8192

/* Returns a pointer to the first "\r\n\r\n" in buf[0..len), or NULL. */
static const char *find_head_end(const char *buf, size_t len) {
    /* TODO */
}

/* Reads until the header block is complete.
   On success returns the total bytes read and sets *head_len to the
   length of the header block including the final "\r\n\r\n".
   Returns 0 if the client closed, -1 on error, -2 if the headers do
   not fit in cap bytes. */
static ssize_t read_request_head(int fd, char *buf, size_t cap,
                                 size_t *head_len) {
    /* TODO */
}

static void handle_client(int fd) {
    char buf[REQ_MAX];
    size_t head_len;
    ssize_t got = read_request_head(fd, buf, sizeof buf, &head_len);
    if (got <= 0) return;

    printf("---- request ----\n%.*s", (int)head_len, buf);

    /* TODO: build the status line and headers with snprintf,
       then write_all the headers and the body. */
}
```

### Test it

```bash
curl -v http://localhost:8080/anything
```

Lines starting with `>` are what curl sent; lines with `<` are your response. Then open `http://localhost:8080/` in a browser and compare what it sends with what curl sent.

### Hints

<details>
<summary>Hint 1</summary>

`read_request_head` is a loop that appends each `read` to the end of the buffer (`buf + used`), then checks whether the end marker has appeared yet. Do not assume the first `read` gets everything.

</details>

<details>
<summary>Hint 2</summary>

```
used = 0
loop:
    if used == cap: return -2
    n = read(fd, buf + used, cap - used)
    n == 0 -> return 0;  n < 0 -> retry on EINTR, else return -1
    used += n
    end = find_head_end(buf, used)
    if end: *head_len = (end - buf) + 4; return used
```

The response you write looks like this, with every line ending in `\r\n`:

```
HTTP/1.1 200 OK
Content-Type: text/plain
Content-Length: 13
Connection: close

Hello, world!
```

</details>

<details>
<summary>Hint 3</summary>

```c
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
```

`find_head_end` rescans from the start after every `read`. That is fine for 8 KB; making it remember where it stopped is an optional improvement.

</details>

### Experiments

Break the response on purpose and watch how curl reacts. You will meet these symptoms again later, so it pays to recognize them now:

- Set `Content-Length` to 50 while sending 13 bytes.
- Set it to 5.
- Use `\n` instead of `\r\n` everywhere.
- Change `text/plain` to `text/html` and send `<h1>Hello</h1>` as the body.

---

## Milestone 3: Parsing requests

**Goal:** split each request into method, target, version and headers. Reply 400 to malformed requests, 501 to methods you do not support, and otherwise send a page that shows the method and path.

**Concepts:** [Parsing untrusted input](http-server-concepts.md#parsing-untrusted-input), [Building responses](http-server-concepts.md#building-responses).

### The data structures

Instead of copying each piece into its own string, you store pointers into the request buffer. A **slice** is a pointer plus a length:

```c
#include <strings.h>   /* strncasecmp */

#define MAX_HEADERS 64

struct slice {
    const char *ptr;
    size_t      len;
};

struct header {
    struct slice name;
    struct slice value;
};

struct request {
    struct slice  method;
    struct slice  target;
    struct slice  version;
    struct header headers[MAX_HEADERS];
    size_t        header_count;
};
```

Slices are not null-terminated, so `strcmp` and `printf("%s")` will not work on them. You need small helpers instead.

### Functions to write

```c
/* 1 if the slice equals the literal exactly, else 0. */
static int slice_eq(struct slice s, const char *lit);

/* Same, ignoring upper/lower case (header names are case-insensitive). */
static int slice_eq_nocase(struct slice s, const char *lit);

/* Returns the value of the first header with this name, or NULL. */
static const struct slice *find_header(const struct request *req,
                                       const char *name);

/* Returns a pointer to the next "\r\n" in [p, end), or NULL. */
static const char *find_crlf(const char *p, const char *end);

/* "GET /path HTTP/1.1" -> method, target, version. 0 ok, -1 malformed. */
static int parse_request_line(const char *p, const char *end,
                              struct request *req);

/* "Name: value" -> appended to req->headers. 0 ok, -1 malformed. */
static int parse_header_line(const char *p, const char *end,
                             struct request *req);

/* Parses the whole header block buf[0..head_len). 0 ok, -1 malformed. */
static int parse_request(const char *buf, size_t head_len,
                         struct request *req);

/* Sends a complete response with the given body. */
static void send_response(int fd, int status, const char *reason,
                          const char *type, const char *body,
                          size_t body_len);

/* Sends a small HTML error page, for example send_error(fd, 404, "Not Found"). */
static void send_error(int fd, int status, const char *reason);
```

### How `parse_request` walks the buffer

```
p   = buf
end = buf + head_len - 2        stop before the final empty line's "\r\n"

line_end = find_crlf(p, end)    first line is the request line
parse_request_line(p, line_end)
p = line_end + 2

while p < end:                  every other line is a header
    line_end = find_crlf(p, end)
    parse_header_line(p, line_end)
    p = line_end + 2
```

### Rules to enforce

- The request line has exactly three parts separated by single spaces.
- The method and target are not empty. The target starts with `/`.
- A header line contains a colon, and the name before it is not empty and has no spaces.
- Leading and trailing spaces or tabs around a header value are trimmed.
- No more than `MAX_HEADERS` headers.

### What `handle_client` decides

```
parse fails                               -> 400 Bad Request
version is not HTTP/1.0 or HTTP/1.1       -> 505 HTTP Version Not Supported
method is not GET or HEAD                 -> 501 Not Implemented
HTTP/1.1 and no Host header               -> 400 Bad Request
read_request_head returned -2             -> 431 Request Header Fields Too Large
otherwise                                 -> 200 with "You asked for GET /path"
```

For `HEAD`, send the same headers, including `Content-Length`, but no body.

### Test it

| Command | Expected |
|---------|----------|
| `curl -i http://localhost:8080/hello` | 200, body mentions `GET /hello` |
| `curl -I http://localhost:8080/hello` | 200 headers, no body |
| `curl -i -X DELETE http://localhost:8080/` | 501 |
| `printf 'GET / HTTP/1.1\r\n\r\n' \| nc -N localhost 8080` | 400 (no Host) |
| `printf 'GARBAGE\r\n\r\n' \| nc -N localhost 8080` | 400 |
| `printf 'GET / HTTP/1.1\r\nBadHeader\r\n\r\n' \| nc -N localhost 8080` | 400 |
| `printf 'GET / HTTP/2.0\r\nHost: x\r\n\r\n' \| nc -N localhost 8080` | 505 |

`-N` tells OpenBSD netcat to close its side after sending. If your `nc` does not know `-N`, try `-q 1`.

### Hints

<details>
<summary>Hint 1</summary>

`memchr(p, ' ', len)` finds the first space in a range that is not null-terminated. Use it to find the two spaces in the request line and the colon in a header.

Debug by printing every parsed piece with `%.*s` before you write the decision logic.

</details>

<details>
<summary>Hint 2</summary>

`parse_header_line`:

```
colon = memchr(p, ':', end - p)
if no colon or colon == p: return -1
name = slice(p, colon - p);  reject if it contains ' ' or '\t'
v = colon + 1
while v < end and *v is ' ' or '\t': v++
e = end
while e > v and e[-1] is ' ' or '\t': e--
value = slice(v, e - v)
if header_count == MAX_HEADERS: return -1
append {name, value}
```

The slice helpers compare the length first, then the bytes:

```c
static int slice_eq(struct slice s, const char *lit) {
    size_t n = strlen(lit);
    return s.len == n && memcmp(s.ptr, lit, n) == 0;
}
```

</details>

<details>
<summary>Hint 3</summary>

```c
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
```

`(struct slice){ ... }` is a **compound literal**: a temporary struct value written inline.

</details>

### Checkpoint questions

- Every slice points into `buf`, which is a local array in `handle_client`. What would go wrong if `parse_request` returned the request to a caller after `buf` went out of scope?
- Why is it fine to ignore headers you do not recognize?

---

## Milestone 4: Serving static files

**Goal:** a real website in `public/` loads in a browser, with correct content types, and nobody can read files outside `public/`.

**Concepts:** [Serving files](http-server-concepts.md#serving-files), [Parsing untrusted input](http-server-concepts.md#parsing-untrusted-input) (percent-decoding).

This is a good moment to split your code into several files (see [Setup](#setup)).

### Make a test site

```bash
mkdir -p public/css
printf '<!doctype html>\n<title>Test</title>\n<link rel="stylesheet" href="/css/site.css">\n<h1>It works</h1>\n<p><a href="/about.html">About</a></p>\n' > public/index.html
printf '<!doctype html>\n<title>About</title>\n<h1>About my server</h1>\n' > public/about.html
printf 'body { font-family: sans-serif; max-width: 40rem; margin: 2rem auto; }\n' > public/css/site.css
```

Copy any PNG or JPEG you have into `public/` and link it from `index.html` too.

### The path pipeline

Each step can reject the request:

```
target "/css/my%20site.css?v=2"
  -> strip the query string         "/css/my%20site.css"
  -> percent-decode                 "/css/my site.css"        bad escape or %00   -> 400
  -> join with the document root    "/home/you/http-server/public/css/my site.css"
  -> realpath()                     resolve "..", "." and symlinks   not found -> 404
  -> prefix check                   still inside the root?           no        -> 403
  -> directory?                     append "/index.html" and check again
  -> open, fstat, send headers, send body (no body for HEAD)
```

Resolve the document root itself with `realpath` once at startup, and keep the result and its length.

### Functions to write

```c
#include <limits.h>    /* PATH_MAX */
#include <stdlib.h>    /* realpath */
#include <fcntl.h>
#include <sys/stat.h>

/* Value of one hex digit, or -1 if c is not a hex digit. */
static int hex_value(char c);

/* Decodes %XX escapes from in[0..len) into out, null-terminated.
   Returns the decoded length, or -1 on a bad escape, %00, or overflow. */
static ssize_t percent_decode(const char *in, size_t len,
                              char *out, size_t cap);

/* Joins root + decoded and canonicalizes into out (PATH_MAX bytes).
   Returns 0 ok, -1 not found, -2 outside the root. */
static int resolve_path(const char *root, size_t root_len,
                        const char *decoded, char *out);

/* MIME type for a path, based on its extension. */
static const char *mime_for(const char *path);

/* Sends the file at path, or only its headers when head_only is set. */
static void serve_file(int client_fd, const char *path, int head_only);
```

`+` is **not** a space in a URL path, only in form data. Leave it as is.

### Test it

| Command | Expected |
|---------|----------|
| browser: `http://localhost:8080/` | styled page, working link, image shows |
| `curl -I http://localhost:8080/css/site.css` | `Content-Type: text/css` |
| `curl -i http://localhost:8080/nope.html` | 404 |
| `curl -i --path-as-is http://localhost:8080/../../etc/passwd` | 403 or 404, never the file |
| `curl -i --path-as-is 'http://localhost:8080/%2e%2e/%2e%2e/etc/passwd'` | 403 or 404 |
| `curl -i 'http://localhost:8080/index.html%00.png'` | 400 |
| `curl -i 'http://localhost:8080/%zz'` | 400 |

`--path-as-is` stops curl from cleaning up `..` itself, so your server really receives the attack.

### Hints

<details>
<summary>Hint 1</summary>

- Query string: `memchr(target.ptr, '?', target.len)` tells you where the path stops.
- `realpath(path, out)` returns `NULL` if the file does not exist. That gives you 404 for free.
- Open the file **first** and then call `fstat` on the open fd. If you `stat` the path and then `open` it, the file could change in between.

</details>

<details>
<summary>Hint 2</summary>

The prefix check must also look at the character right after the root, otherwise `/srv/www-private` would pass as being inside `/srv/www`:

```
strncmp(out, root, root_len) == 0  and  (out[root_len] == '/' or out[root_len] == '\0')
```

`mime_for`: find the last `.` with `strrchr`, then compare the extension against a table of `{ ".html", "text/html; charset=utf-8" }` entries with `strcasecmp`. Return `application/octet-stream` if nothing matches.

</details>

<details>
<summary>Hint 3</summary>

```c
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
```

`serve_file` is yours. Its shape: `open`, `fstat`, check `S_ISREG`, build headers with `snprintf` using `(long long)st.st_size` and `%lld`, `write_all` the headers, then loop `read` into a 16 KB buffer and `write_all` each chunk. Always `close` the file, on every path out of the function.

</details>

---

## Milestone 5: Hardening

**Goal:** no input from a client can crash the server, hang it, or make it leak.

**Concepts:** [Errors, signals and timeouts](http-server-concepts.md#errors-signals-and-timeouts), [Security mindset](http-server-concepts.md#security-mindset).

This milestone has little new code. It is about attacking your own server and fixing what breaks.

### Add these three things

1. At the top of `main`: `signal(SIGPIPE, SIG_IGN);` so a client that disappears mid-response cannot kill the server.
2. Right after `accept`: a 10-second receive timeout with `setsockopt(..., SO_RCVTIMEO, ...)` (see the concepts guide). Treat a timed-out `read` like an error: close the connection.
3. A log line per request, to stderr: client IP, method, target, status. Use `getpeername` or the `struct sockaddr_storage` that `accept` can fill, and `inet_ntop` to print the address.

### Attack your server

Keep it running with sanitizers on (the Makefile already does this). Run each attack and check that the server is still alive afterwards.

| Attack | Command | Expected |
|--------|---------|----------|
| Huge header | ``curl -i -H "X-Big: $(head -c 20000 /dev/zero \| tr '\0' a)" http://localhost:8080/`` | 431 |
| Random bytes | `head -c 4000 /dev/urandom \| nc -N localhost 8080` | 400 or closed, no crash |
| Only `\n` line endings | `printf 'GET / HTTP/1.1\nHost: x\n\n' \| nc -N localhost 8080` | 400 or timeout, no crash |
| Client hangs up mid-file | `head -c 50M /dev/urandom > public/big.bin` then `curl -s http://localhost:8080/big.bin \| head -c 10 > /dev/null` | server keeps running |
| Silent client | `nc localhost 8080` and type nothing | closed after about 10 s |
| 1000 requests | `for i in $(seq 1000); do curl -s -o /dev/null http://localhost:8080/; done` | memory stays flat |

For the last one, watch memory with `ps -o rss= -p $(pgrep server)` before and after, or stop the server with Ctrl+C and read what AddressSanitizer reports about leaks.

### Hints

<details>
<summary>Hint 1</summary>

If the server dies without a message, it is almost always `SIGPIPE`. If AddressSanitizer prints a report, the first "READ of size" or "WRITE of size" line plus the stack trace below it point at the exact line.

</details>

<details>
<summary>Hint 2</summary>

Checklist when a test fails:

- Is every buffer write bounded by the buffer's size?
- Does every function that opens a file close it on **every** return path?
- Are you calling `exit()` or `abort()` anywhere for a client's mistake? Replace it with an error response.

</details>

<details>
<summary>Hint 3</summary>

Printing the client address after `accept`:

```c
#include <arpa/inet.h>

struct sockaddr_storage addr;
socklen_t addr_len = sizeof addr;
int client_fd = accept(listen_fd, (struct sockaddr *)&addr, &addr_len);

char ip[INET6_ADDRSTRLEN] = "?";
if (addr.ss_family == AF_INET) {
    inet_ntop(AF_INET, &((struct sockaddr_in *)&addr)->sin_addr,
              ip, sizeof ip);
} else if (addr.ss_family == AF_INET6) {
    inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&addr)->sin6_addr,
              ip, sizeof ip);
}
```

Pass `ip` into `handle_client` so it can include it in the log line.

</details>

---

## Milestone 6: Many clients with threads

**Goal:** one slow or silent client no longer blocks everyone else.

**Concepts:** [Concurrency models](http-server-concepts.md#concurrency-models).

### The plan

1. Your per-client work already lives in `handle_client(fd, ...)`. Keep it that way.
2. Write a thread entry function `client_thread(void *arg)` that unpacks its argument, calls `handle_client`, closes the fd and frees the argument.
3. In the accept loop, `malloc` a small struct holding the fd and the client IP, start a thread with it, and detach the thread.
4. Protect anything shared. Your log output is shared: two threads printing at once can mix their lines.

### Skeleton

```c
#include <pthread.h>

struct client_info {
    int  fd;
    char ip[INET6_ADDRSTRLEN];
};

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

static void *client_thread(void *arg) {
    /* TODO: copy what you need out of arg, free(arg),
       handle_client(...), close(fd), return NULL. */
}

/* in main's accept loop, after accept() succeeds: */
    /* TODO: malloc a struct client_info, fill it,
       pthread_create(&tid, NULL, client_thread, info),
       pthread_detach(tid).
       If either call fails: close the fd and free info. */
```

Wrap every log `fprintf` in `pthread_mutex_lock(&log_lock)` and `pthread_mutex_unlock(&log_lock)`.

### Test it

Terminal A: `nc localhost 8080` and type nothing.
Terminal B: `curl -i http://localhost:8080/`. It must answer immediately.

Then load test:

```bash
wrk -t4 -c100 -d10s http://localhost:8080/
```

(`wrk` is in most package managers; `ab -n 5000 -c 100 http://localhost:8080/` works too.) Watch for "Socket errors" in the output and any sanitizer report from the server.

### Hints

<details>
<summary>Hint 1</summary>

Never pass `&client_fd` to the thread. The accept loop reuses that variable for the next client, possibly before the new thread has read it. Each thread needs its own heap copy.

</details>

<details>
<summary>Hint 2</summary>

Is any of your code using a `static` local buffer or a global that changes per request? Every thread would share it. Look for `static char` inside functions. `strtok` and `localtime` also keep hidden state; use `strtok_r` and `localtime_r` instead.

</details>

<details>
<summary>Hint 3</summary>

```c
static void *client_thread(void *arg) {
    struct client_info info = *(struct client_info *)arg;
    free(arg);
    handle_client(info.fd, info.ip);
    close(info.fd);
    return NULL;
}
```

Copying the struct into a local first means you can `free` the heap copy straight away and never think about it again.

</details>

### Stretch: a thread pool

Start N worker threads at startup that all wait on a shared queue of fds. The accept loop pushes into the queue; workers pop and call `handle_client`. You need:

```c
struct fd_queue {
    int             fds[256];
    size_t          head, tail, count;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
    pthread_cond_t  not_full;
};
```

`push` waits on `not_full` while `count` is 256; `pop` waits on `not_empty` while `count` is 0. Always wait inside a `while` loop, not an `if`, because a thread can wake up without the condition being true.

---

## Milestone 7: Keep-alive

**Goal:** one connection can carry many requests, including several sent back to back without waiting (pipelining).

**Concepts:** [Connection lifetime and keep-alive](http-server-concepts.md#connection-lifetime-and-keep-alive), framing.

### What changes

So far each connection reads one request into a fresh buffer and closes. Now the buffer must live across requests, because one `read` can contain the end of request 1 *and* the start of request 2. Those leftover bytes must be kept for the next round.

```c
struct conn {
    int    fd;
    char   ip[INET6_ADDRSTRLEN];
    char   buf[REQ_MAX];
    size_t used;       /* bytes currently in buf */
};
```

### The new loop inside `handle_client`

```
for (;;):
    r = conn_fill_head(&c, &head_len)        check buf first, read only if needed
    if r <= 0: break                         closed, error, timeout or too large
    parse_request(c.buf, head_len, &req)
    body_len = Content-Length (0 if absent)
    keep = wants_keep_alive(&req)
    respond, with "Connection: keep-alive" or "Connection: close"
    conn_skip_body(&c, head_len, body_len)   discard the body; also drops the head
    if not keep: break
```

### Functions to write

```c
/* Like read_request_head, but first checks whether c->buf already holds
   a complete header block from an earlier read. */
static ssize_t conn_fill_head(struct conn *c, size_t *head_len);

/* Removes the first n bytes of c->buf, sliding the rest to the front. */
static void conn_consume(struct conn *c, size_t n);

/* Drops the header block and a body of body_len bytes, part of which
   may still be unread on the socket. 0 ok, -1 error. */
static int conn_skip_body(struct conn *c, size_t head_len, size_t body_len);

/* Parses a Content-Length value. 0 ok, -1 invalid. */
static int parse_content_length(struct slice v, size_t *out);

/* HTTP/1.1: keep alive unless "Connection: close".
   HTTP/1.0: close (simplest correct choice). */
static int wants_keep_alive(const struct request *req);
```

Also reject any request that has a `Transfer-Encoding` header with 501 for now. You are not decoding chunked bodies yet.

### The trap in this milestone

Every slice in `req` points into `c->buf`. `conn_consume` moves bytes with `memmove`, so after it runs, **all those slices point at the wrong data**. Finish everything that uses `req` (logging included) before consuming. AddressSanitizer will not always catch this, because the memory is still valid, just wrong.

### Test it

```bash
curl -v http://localhost:8080/ http://localhost:8080/about.html
```

Look for "Re-using existing connection" in the output. Then pipelining: two requests in one write.

```bash
printf 'GET / HTTP/1.1\r\nHost: x\r\n\r\nGET /about.html HTTP/1.1\r\nHost: x\r\nConnection: close\r\n\r\n' | nc localhost 8080
```

You should get two complete responses, in order, and then the connection closes.

### Hints

<details>
<summary>Hint 1</summary>

`conn_fill_head` is your old `read_request_head` with one extra step at the top: before reading, call `find_head_end(c->buf, c->used)`. If the leftover bytes already contain a full request, return without touching the socket. If you skip this, a pipelined second request waits forever for a `read` that never comes.

</details>

<details>
<summary>Hint 2</summary>

Slices are not null-terminated, so `strtoul` could read past the end of the value. Parse the digits yourself: reject an empty value and any non-digit, and limit it to 18 digits so `n * 10` cannot overflow a 64-bit `size_t`.

For `conn_skip_body`, there are two cases:

```
in_buf = c->used - head_len                 body bytes already received
if body_len <= in_buf:
    conn_consume(c, head_len + body_len)    all here; done
else:
    conn_consume(c, c->used)                drop everything we have
    left = body_len - in_buf
    read and throw away "left" more bytes from the socket
```

</details>

<details>
<summary>Hint 3</summary>

```c
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
```

`memmove`, not `memcpy`: the source and destination overlap, and only `memmove` is defined for that.

</details>

---

## Milestone 8: Event loop with epoll

**Goal:** a single thread serves thousands of connections at once.

**Concepts:** [Non-blocking I/O and event loops](http-server-concepts.md#non-blocking-io-and-event-loops).

This is the biggest rewrite. Keep your milestone 7 server in a separate folder or git branch so you can compare.

### The mental shift

In milestones 1 to 7, each connection's code runs top to bottom and simply waits inside `read`. In an event loop nothing may wait. Instead:

- The kernel tells you a socket is **readable** or **writable**.
- You do as much as you can without blocking, save your progress in the connection's struct, and return.
- Later, another event lets you continue from where you saved.

So each connection becomes a small **state machine**:

```
READING  --full request parsed-->  WRITING  --all bytes sent-->  READING (keep-alive)
                                                             \-> closed (Connection: close)
```

### Data structures

You can no longer `write_all` straight to the socket: a write may stop halfway with `EAGAIN`. Build each response into a growable buffer instead, and send it bit by bit as the socket allows.

```c
#include <fcntl.h>
#include <sys/epoll.h>

struct buf {
    char  *data;
    size_t len;
    size_t cap;
};

enum conn_state { CONN_READING, CONN_WRITING };

struct conn {
    int             fd;
    enum conn_state state;
    char            in[REQ_MAX];
    size_t          in_used;
    struct buf      out;        /* the response being sent */
    size_t          out_sent;
    size_t          consumed;   /* request bytes to drop after the response */
    int             keep_alive;
};
```

Refactor your response code from milestones 3 and 4 so it appends to a `struct buf` instead of writing to an fd. Then both the threaded and the epoll server can use it.

### Skeleton

```c
static int  set_nonblocking(int fd);
static int  buf_append(struct buf *b, const void *src, size_t n);
static void accept_all(int ep, int listen_fd);
static void conn_close(int ep, struct conn *c);
static void on_readable(int ep, struct conn *c);
static void on_writable(int ep, struct conn *c);

int main(void) {
    signal(SIGPIPE, SIG_IGN);
    int listen_fd = open_listener(PORT);
    if (listen_fd < 0 || set_nonblocking(listen_fd) < 0) return 1;

    int ep = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.ptr = NULL };
    epoll_ctl(ep, EPOLL_CTL_ADD, listen_fd, &ev);   /* NULL marks the listener */

    struct epoll_event events[128];
    for (;;) {
        int n = epoll_wait(ep, events, 128, -1);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++) {
            struct conn *c = events[i].data.ptr;
            if (c == NULL) {
                accept_all(ep, listen_fd);
            } else if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                conn_close(ep, c);
            } else if (c->state == CONN_READING) {
                on_readable(ep, c);
            } else {
                on_writable(ep, c);
            }
        }
    }
    return 0;
}
```

### What the handlers do

**`on_readable`**

```
loop: read into c->in + c->in_used
    n > 0            -> in_used += n; if in_used == REQ_MAX stop reading
    n == 0           -> conn_close; return
    EAGAIN           -> stop reading (nothing more for now)
    other error      -> conn_close; return
if find_head_end(c->in, c->in_used) found a full header block:
    parse it, build the response into c->out
    c->consumed = head length; c->keep_alive = ...
    c->state = CONN_WRITING
    epoll_ctl MOD: watch EPOLLOUT instead of EPOLLIN
else if buffer full -> respond 431 and close afterwards
```

**`on_writable`**

```
loop while out_sent < out.len:
    n = write(fd, out.data + out_sent, out.len - out_sent)
    n > 0  -> out_sent += n
    EAGAIN -> return (wait for the next EPOLLOUT)
    error  -> conn_close; return
response finished:
    out.len = 0; out_sent = 0
    drop c->consumed bytes from the front of c->in (memmove)
    if not keep_alive -> conn_close; return
    c->state = CONN_READING; epoll_ctl MOD: watch EPOLLIN
    if c->in already holds another full request -> process it now
```

That last line matters: epoll only reports **new** data. A pipelined request already sitting in your buffer never triggers another event.

### Test it

```bash
ulimit -n 4096
wrk -t4 -c1000 -d15s http://localhost:8080/
```

Run `ulimit -n 4096` in the server's terminal before starting it, so the process may open enough fds. Compare requests per second with your threaded server.

### Hints

<details>
<summary>Hint 1</summary>

Two common reasons the server uses 100% CPU while doing nothing: you are watching `EPOLLOUT` on a connection with nothing to send (an idle socket is almost always writable), or you forgot to switch back to `EPOLLIN` after finishing a response.

</details>

<details>
<summary>Hint 2</summary>

`accept_all` must loop until `accept` returns `EAGAIN`. Several clients may be waiting, and you only get one event for all of them.

`conn_close` order: `epoll_ctl(ep, EPOLL_CTL_DEL, c->fd, NULL)`, `close(c->fd)`, `free(c->out.data)`, `free(c)`. After this, do not touch `c` again in the same loop iteration.

</details>

<details>
<summary>Hint 3</summary>

```c
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
        struct conn *c = calloc(1, sizeof *c);
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
```

</details>

### Stretch goals

- **Idle timeouts:** `SO_RCVTIMEO` means nothing for non-blocking sockets. Store a `last_active` time in each `conn`, give `epoll_wait` a 1000 ms timeout, and close connections that have been quiet too long. A linked list ordered by `last_active` makes the sweep cheap.
- **Large files:** instead of loading a whole file into `out`, keep the file's fd and an offset in `conn`, and call `sendfile` from `on_writable` until it returns `EAGAIN`.

---

## Milestone 9: Make it yours

Pick at least one. Each uses something you have already practised.

### Routing table

Map method + path to a handler function with an array of structs holding function pointers:

```c
typedef void (*handler_fn)(struct buf *out, const struct request *req);

struct route {
    const char *method;
    const char *path;
    handler_fn  fn;
};

static const struct route routes[] = {
    { "GET", "/health", handle_health },
    { "GET", "/time",   handle_time   },
};

```
Look up the route first; fall back to static files when nothing matches. Add a 405 response when the path exists but the method does not.

### Directory listings

When a directory has no `index.html`, generate an HTML list of its entries with `opendir`/`readdir`. Escape `<`, `>`, `&` and `"` in file names before putting them in HTML, or a file called `<script>.txt` becomes an attack.

### `POST /echo`

Read the body (now you need it, instead of discarding it) and send it back. Enforce a maximum size with 413.

### Access log

Write one line per request to a file, in the Common Log Format used by Apache and nginx. Then point a log analysis tool such as GoAccess at it.

### Config file

Read the port, document root and limits from a small `key = value` file at startup, using `fgets` and `sscanf`.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|-------------|
| `bind: Address already in use` | the old server is still running (`ss -tlnp \| grep 8080`), or `SO_REUSEADDR` is missing |
| `curl: (7) Failed to connect` | server not running, wrong port, or it crashed; check its terminal |
| curl hangs after receiving headers | `Content-Length` is bigger than the body you sent |
| `curl: (18) transfer closed with outstanding read data remaining` | you closed before sending all `Content-Length` bytes |
| curl says the response is malformed or "HTTP/0.9" | status line wrong, or `\n` instead of `\r\n` |
| browser shows HTML source as plain text | wrong `Content-Type` |
| browser keeps spinning | missing `Content-Length`, or you promised keep-alive but stopped reading |
| server exits silently when a client disconnects | `SIGPIPE`; ignore it at startup |
| garbage after printed request parts | printing a slice with `%s` instead of `%.*s` |
| `accept: Too many open files` | an fd is not closed on some path; check every early `return` |
| wrong headers on the second keep-alive request | slices used after `conn_consume` moved the buffer |
| `heap-buffer-overflow` from AddressSanitizer | a loop or copy ran past a buffer; the stack trace names the line |
| server at 100% CPU while idle (epoll) | watching `EPOLLOUT` with nothing to send |
| `/favicon.ico` 404s in your log | normal: browsers ask for it automatically; add one to `public/` |

When you are stuck, check three things before anything else: run under `strace -f ./server` to see the system calls, read `curl -v` output line by line, and read the first AddressSanitizer report.

