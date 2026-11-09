# Building an HTTP Server in C: The Concepts

This is not a tutorial. There is no finished server to copy here. Instead, each section introduces one concept you need, shows a small snippet of how it looks in C, points out the traps, and ends with questions you should be able to answer before moving on. You assemble the server yourself.

**Prerequisites:** chapters 5 to 8 of the [study guide](c-study-guide.md) (pointers, structs, I/O, file descriptors and `fork`). Linux or macOS. Examples use POSIX APIs, plus `epoll` and `sendfile` for Linux.

## Contents

- [The big picture](#the-big-picture)
- [Networking foundations](#networking-foundations)
- [Sockets](#sockets)
- [TCP is a byte stream](#tcp-is-a-byte-stream)
- [The HTTP message format](#the-http-message-format)
- [Parsing untrusted input](#parsing-untrusted-input)
- [Building responses](#building-responses)
- [Serving files](#serving-files)
- [Connection lifetime and keep-alive](#connection-lifetime-and-keep-alive)
- [Errors, signals and timeouts](#errors-signals-and-timeouts)
- [Concurrency models](#concurrency-models)
- [Non-blocking I/O and event loops](#non-blocking-io-and-event-loops)
- [Resource ownership](#resource-ownership)
- [Security mindset](#security-mindset)
- [Tools for testing and debugging](#tools-for-testing-and-debugging)
- [Milestones](#milestones)
- [Going further](#going-further)
- [References](#references)

---

## The big picture

Every HTTP server, from a 200-line toy to nginx, does the same loop:

1. Ask the OS for a socket and claim a port.
2. Wait for a client to connect.
3. Read bytes from the connection until a full request has arrived.
4. Parse those bytes into a method, a path, headers and maybe a body.
5. Decide what to do (serve a file, run a handler, return an error).
6. Write a correctly formatted response back.
7. Close the connection, or keep it open for the next request.

The concepts below map onto those steps. The hard parts are not the happy path. They are partial reads, malformed input, slow clients, and handling many clients at once.

---

## Networking foundations

### Client, server, address, port

A **server** waits for connections. A **client** starts them. A connection is identified by four values: client IP, client port, server IP, server port. Your server picks one port, such as 8080, and the OS keeps every connection to it separate by the client's IP and port.

Ports below 1024 need root privileges on most systems. Use 8080 or similar while learning.

### TCP vs UDP

HTTP/1.1 runs on **TCP**, which gives you:

- **Reliable** delivery: lost packets are re-sent for you.
- **Ordered** bytes: data arrives in the order it was sent.
- A **connection**: a setup handshake, then a two-way stream, then a teardown.

UDP gives none of these. You will not use it here.

### Byte order

Different CPUs store multi-byte integers in different orders. Network protocols use **big-endian** ("network byte order"). Convert before putting numbers into address structs:

```c
#include <arpa/inet.h>

uint16_t net_port = htons(8080);   // host to network, short
uint16_t back     = ntohs(net_port);
```

Forgetting `htons` is a classic bug. The server silently listens on the wrong port.

### Check yourself

- Why can two different browsers both connect to port 8080 on your server at the same time?
- What does TCP guarantee, and what does it *not* guarantee about how your `read` calls are split?

---

## Sockets

A **socket** is the OS's handle for a network endpoint. In POSIX it is just a file descriptor, so `read`, `write` and `close` work on it, as in chapter 8 of the study guide.

### The server lifecycle

| Call | What it does |
|------|--------------|
| `socket()` | creates an unconnected endpoint |
| `setsockopt()` | sets options, such as allowing a quick restart on the same port |
| `bind()` | attaches the socket to an address and port |
| `listen()` | marks it as passive: it will accept connections |
| `accept()` | blocks until a client connects, then returns a **new** fd for that client |
| `read()`/`write()` | exchange bytes on the client fd |
| `close()` | ends that conversation |

The key idea: the **listening socket** never carries data. Each `accept` hands you a separate **connected socket** per client.

### Getting an address: `getaddrinfo`

Filling in `struct sockaddr_in` by hand works, but `getaddrinfo` is the modern way. It handles IPv4 and IPv6 and fills the structs for you:

```c
#include <netdb.h>
#include <sys/socket.h>
#include <string.h>

struct addrinfo hints, *res;
memset(&hints, 0, sizeof hints);
hints.ai_family   = AF_UNSPEC;     // IPv4 or IPv6
hints.ai_socktype = SOCK_STREAM;   // TCP
hints.ai_flags    = AI_PASSIVE;    // "my own address", for servers

int rc = getaddrinfo(NULL, "8080", &hints, &res);
if (rc != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rc));
}
/* res is a linked list: try each entry with socket() + bind() */
freeaddrinfo(res);
```

Notice that `res` is a linked list of structs, the same pattern as chapter 6. You walk it until one entry works.

### `SO_REUSEADDR`

When you stop and restart your server, `bind` often fails with "Address already in use" for a minute. The old connections are in the `TIME_WAIT` state. Set this option before `bind`:

```c
int yes = 1;
setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
```

### The backlog

`listen(fd, backlog)` sets how many finished handshakes the kernel will queue while you are busy and not calling `accept`. It is a queue size, not a limit on total clients.

### Check yourself

- Why does `accept` return a new file descriptor instead of reusing the listening one?
- What is in the `struct sockaddr` that `accept` can fill in, and how would you print the client's IP? (Look up `inet_ntop`.)
- What happens if you `bind` to `127.0.0.1` instead of all addresses? Who can reach you?

---

## TCP is a byte stream

This is the most important concept in the guide.

TCP does not deliver *messages*. It delivers a stream of bytes. One `write` from a client can show up as three `read`s on your side, and three `write`s can arrive in one `read`. Your code must not assume a request arrives in one piece.

### Partial reads

```c
ssize_t n = read(client_fd, buf, sizeof buf);
// n == 0   -> the client closed the connection
// n == -1  -> error (check errno; EINTR means "try again")
// n > 0    -> you got n bytes, which may be any part of the request
```

So you **accumulate** bytes into a buffer and look for a boundary that tells you the message is complete. For HTTP headers that boundary is the blank line, `\r\n\r\n`.

### Partial writes

`write` may also send fewer bytes than you asked for, especially with large responses. The usual fix is a helper that loops until everything is out:

```c
#include <errno.h>
#include <unistd.h>

int write_all(int fd, const char *data, size_t len) {
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
```

This is chapter 5's pointer arithmetic doing real work: `data` walks forward through the buffer.

### Framing

**Framing** means knowing where one message ends and the next begins. HTTP/1.1 uses:

- `\r\n\r\n` to end the header section.
- `Content-Length: N` to say the body is exactly N bytes.
- Or `Transfer-Encoding: chunked`, where the body comes in size-prefixed pieces.

### Check yourself

- A client sends `GET / HTTP/1.1\r\nHost: x\r\n\r\n`. Your first `read` returns only `GET / HT`. What should your code do next?
- If a single `read` contains the end of one request and the start of the next one, where do the leftover bytes go?

---

## The HTTP message format

HTTP/1.x is plain text, so you can type it by hand with `nc` (see the tools section). Lines end with **CRLF** (`\r\n`), not just `\n`.

### A request

```http
GET /docs/intro.html HTTP/1.1
Host: localhost:8080
User-Agent: curl/8.10.1
Accept: */*

```

Its parts:

- **Request line:** method, target, version, separated by single spaces.
- **Header fields:** `Name: value`, one per line. Names are case-insensitive.
- **Blank line:** marks the end of the headers.
- **Body:** optional. Present for `POST`/`PUT` and sized by `Content-Length`.

### A response

```http
HTTP/1.1 200 OK
Content-Type: text/html; charset=utf-8
Content-Length: 13

Hello, world!
```

- **Status line:** version, three-digit code, reason phrase.
- Headers, a blank line, then exactly `Content-Length` bytes of body.

### Methods to support first

`GET` (fetch), `HEAD` (same as GET but no body; good test of your logic). Add `POST` later once you handle bodies.

### Status codes you will actually use

| Code | When |
|------|------|
| 200 OK | it worked |
| 301 / 302 | redirect (for example `/docs` to `/docs/`) |
| 304 Not Modified | caching (later) |
| 400 Bad Request | you could not parse the request |
| 403 Forbidden | path exists but is not allowed |
| 404 Not Found | no such resource |
| 405 Method Not Allowed | method not supported for this path |
| 413 Content Too Large | body bigger than you allow |
| 431 Request Header Fields Too Large | headers bigger than your buffer |
| 500 Internal Server Error | your bug |
| 501 Not Implemented | method you do not support at all |

### HTTP/1.1 rules worth knowing early

- A `Host` header is required. Reply 400 if it is missing.
- Connections are **persistent by default** unless someone sends `Connection: close`.
- You may answer with `HTTP/1.1` even if the client spoke `HTTP/1.0`.

### Check yourself

- Why does `HEAD` still need a correct `Content-Length`?
- What should your server do with a header it does not recognise? (Answer: ignore it. Unknown headers are normal.)

---

## Parsing untrusted input

Every byte from the network comes from a stranger. Parsing it is where most C servers get security bugs.

### Fixed-size buffers with explicit limits

Pick limits and enforce them: for example 8 KB for the whole header section, 100 headers maximum, 1 MB maximum body. When a limit is hit, reply with an error (431, 413) and close. Never let input decide how much you write into a buffer.

### Tokenizing without copying

You do not need to copy every piece into new strings. A common design keeps the request in one buffer and stores **pointers plus lengths** into it:

```c
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
    struct header headers[64];
    size_t        header_count;
};
```

This avoids `malloc` per field and makes ownership simple: everything lives as long as the buffer. It also means the slices are **not** null-terminated, so you cannot pass them to `strcmp`. Write small helpers such as `slice_eq(struct slice, const char *)` and a case-insensitive version for header names.

### State machines

A parser that may receive input in fragments is easiest to write as a **state machine**: a variable records where you are, and each incoming byte moves you to the next state.

```c
enum parse_state {
    PS_METHOD,
    PS_TARGET,
    PS_VERSION,
    PS_HEADER_NAME,
    PS_HEADER_VALUE,
    PS_HEADERS_DONE,
    PS_BODY,
    PS_COMPLETE,
    PS_ERROR
};
```

The simpler alternative, fine for a first version, is: keep reading until `\r\n\r\n` appears in the buffer, then parse the whole header block in one pass. Start with that, and move to a state machine when you add keep-alive or non-blocking I/O.

### Numbers

Use `strtol`/`strtoul` with an end pointer and a range check for `Content-Length`, never `atoi`. `atoi("abc")` returns 0 and `atoi("99999999999")` is undefined.

### Percent-decoding

Targets can contain escapes: `/my%20file.txt` means `/my file.txt`. You need a decoder that turns `%XX` into one byte, and that rejects bad sequences such as `%G1`. Decode **before** you check the path for safety, or `%2e%2e` will sneak past your `..` filter.

### Check yourself

- What should happen if a header line has no colon?
- If `Content-Length` says `-5`, or appears twice with different values, what do you reply?
- Why is it dangerous to check for `..` in the path before percent-decoding?

---

## Building responses

### Formatting headers safely

`snprintf` into a buffer, and check its return value. If it is greater than or equal to the buffer size, the output was cut off:

```c
char head[512];
int n = snprintf(head, sizeof head,
                 "HTTP/1.1 %d %s\r\n"
                 "Content-Type: %s\r\n"
                 "Content-Length: %zu\r\n"
                 "\r\n",
                 status, reason, mime, body_len);
if (n < 0 || (size_t)n >= sizeof head) {
    /* header did not fit */
}
```

### A lookup table for status text

A small array of structs (chapter 6) maps codes to reason phrases. The same idea maps file extensions to MIME types:

```c
struct mime_entry { const char *ext; const char *type; };

static const struct mime_entry mime_types[] = {
    { ".html", "text/html; charset=utf-8" },
    { ".css",  "text/css" },
    { ".js",   "text/javascript" },
    { ".png",  "image/png" },
    { ".jpg",  "image/jpeg" },
    { ".svg",  "image/svg+xml" },
};
```

Unknown extensions get `application/octet-stream`.

### Useful headers to send

- `Content-Length` (required unless you use chunked encoding).
- `Content-Type`.
- `Date` (use `strftime` with GMT; format described in RFC 9110).
- `Server` (optional, your server's name).
- `Connection: close` when you are about to close.

### Check yourself

- Why should error pages (404, 500) also have a correct `Content-Length`?
- How do you send a response whose length you do not know ahead of time?

---

## Serving files

### Mapping a URL to a path

The URL `/css/site.css` should become something like `./public/css/site.css`. The folder `./public` is your **document root**. Requests for a directory usually serve `index.html` inside it.

### Path traversal

The most famous static-server bug. A request for `/../../etc/passwd` must never escape the document root. Robust approach:

1. Percent-decode the target.
2. Join it to the document root.
3. Resolve it with `realpath()`, which removes `..`, `.` and symlinks.
4. Check that the result still starts with the resolved document root followed by `/`.

Reject anything that fails with 403 or 404.

### Getting size and type

`stat()` (chapter 8) tells you whether the path is a regular file or a directory, and its size for `Content-Length`. Use `st_mtime` later for `Last-Modified` and caching.

### Sending the bytes

Simple way: `open` the file, then loop `read` into a buffer and `write_all` to the socket.

Fast way on Linux: `sendfile()` copies from a file to a socket inside the kernel, without passing through your buffer:

```c
#include <sys/sendfile.h>

off_t offset = 0;
while (offset < st.st_size) {
    ssize_t sent = sendfile(client_fd, file_fd, &offset, st.st_size - offset);
    if (sent <= 0) break;   /* check errno; handle EINTR / EAGAIN */
}
```

### Check yourself

- Your document root is `/srv/www`. Why is a plain "starts with `/srv/www`" check not enough? (Think about `/srv/www-private`.)
- What should a request for a directory without a trailing slash do?

---

## Connection lifetime and keep-alive

Opening a TCP connection costs a round trip. HTTP/1.1 lets the client reuse one connection for many requests.

To support this you must:

- Know exactly where each request ends (framing again).
- Keep leftover bytes from the last `read` as the start of the next request.
- Reset your parser state between requests.
- Close after `Connection: close`, after an error you cannot recover from, or after a period of inactivity.

A simple first version can send `Connection: close` on every response and close afterwards. Add keep-alive as a later milestone.

### Pipelining

A client may send several requests without waiting for answers. You must answer them in order. If your parser keeps leftovers correctly, this mostly works for free.

### Check yourself

- After you finish a response, your buffer holds 40 unprocessed bytes. What do you do with them?
- How long should an idle keep-alive connection stay open, and what resource does it hold meanwhile?

---

## Errors, signals and timeouts

### `errno` and `EINTR`

System calls report failure with `-1` and set `errno`. `EINTR` means a signal interrupted the call; just retry it. Other errors on a client socket usually mean "drop this client" and must **not** crash the whole server.

### `SIGPIPE`

Writing to a socket the client already closed sends your process `SIGPIPE`, which kills it by default. Every server must handle this. Either ignore it once at startup:

```c
#include <signal.h>
signal(SIGPIPE, SIG_IGN);   /* write() now returns -1 with errno == EPIPE */
```

Or use `send(fd, buf, len, MSG_NOSIGNAL)` on Linux instead of `write`.

### Timeouts

A client that connects and never sends anything ties up a connection forever. With blocking sockets, set a receive timeout:

```c
#include <sys/time.h>

struct timeval tv = { .tv_sec = 10, .tv_usec = 0 };
setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
```

After it fires, `read` returns -1 with `errno` set to `EAGAIN` or `EWOULDBLOCK`.

### Graceful shutdown

Handle `SIGINT`/`SIGTERM` by setting a `volatile sig_atomic_t` flag that the main loop checks, so you can close sockets and free memory cleanly. Do very little inside the signal handler itself; most library functions are not safe to call there.

### Check yourself

- Why must a failed `write` to one client never call `exit()`?
- What does a client that sends one byte every 9 seconds do to a 10-second timeout? (Look up "Slowloris".)

---

## Concurrency models

A server that handles one client at a time freezes for everyone while one slow client is connected. There are four classic ways out. Try at least two to feel the trade-offs.

| Model | Idea | Good | Bad |
|-------|------|------|-----|
| Iterative | one client at a time | simplest; great for milestone 1 | one slow client blocks all |
| Process per connection | `fork()` after `accept` | isolation: a crash kills one child | heavy; no shared memory |
| Thread per connection | `pthread_create` after `accept` | lighter; shared memory | data races; thousands of threads hurt |
| Thread pool | fixed workers take fds from a queue | bounded resources | needs a thread-safe queue |
| Event loop | one thread, many non-blocking sockets | scales to thousands of connections | parsing must be resumable |

### Processes

After `fork`, the child handles the client and exits. The parent closes its copy of the client fd and goes back to `accept`. Finished children become **zombies** until reaped; use `waitpid(-1, NULL, WNOHANG)` in a `SIGCHLD` handler, or set `SIGCHLD` to `SIG_IGN` on Linux.

### Threads

```c
#include <pthread.h>

void *handle_client(void *arg) {
    int fd = *(int *)arg;
    free(arg);
    /* ... serve the client ... */
    close(fd);
    return NULL;
}

/* in the accept loop */
int *fdp = malloc(sizeof *fdp);
*fdp = client_fd;
pthread_t t;
pthread_create(&t, NULL, handle_client, fdp);
pthread_detach(t);
```

Why `malloc` the fd? Passing `&client_fd` would let the next `accept` overwrite it before the thread reads it. That is a **race condition**.

Compile with `-pthread`. Anything shared between threads (a log file, a counter, a cache) needs a `pthread_mutex_t` or atomics.

### Thread pools

A pool needs a **producer-consumer queue**: the accept loop pushes fds, workers pop them. Protect the queue with a mutex, and let workers sleep with a condition variable (`pthread_cond_wait`) when it is empty. This is a great data-structures exercise on its own.

### Check yourself

- In the fork model, why must the parent close the client fd and the child close the listening fd?
- In the thread model, what goes wrong if two threads call a function that uses a `static` buffer?

---

## Non-blocking I/O and event loops

This is how high-performance servers handle tens of thousands of connections on one thread.

### Non-blocking sockets

```c
#include <fcntl.h>

int flags = fcntl(fd, F_GETFL, 0);
fcntl(fd, F_SETFL, flags | O_NONBLOCK);
```

Now `read`, `write` and `accept` never wait. If they cannot make progress they return -1 with `errno == EAGAIN`. That means "nothing right now, ask again later".

### Readiness notification

Instead of asking every socket in turn, you ask the kernel "tell me which of these fds are ready". Three generations of the same idea:

- `select()`: oldest, limited to about 1024 fds.
- `poll()`: no fd limit, portable, scans the whole list each time.
- `epoll` (Linux) / `kqueue` (BSD, macOS): scale to huge numbers of fds.

```c
#include <sys/epoll.h>

int ep = epoll_create1(0);

struct epoll_event ev = { .events = EPOLLIN, .data.fd = listen_fd };
epoll_ctl(ep, EPOLL_CTL_ADD, listen_fd, &ev);

struct epoll_event ready[64];
for (;;) {
    int n = epoll_wait(ep, ready, 64, -1);
    for (int i = 0; i < n; i++) {
        int fd = ready[i].data.fd;
        if (fd == listen_fd) {
            /* accept new clients, make them non-blocking, add to ep */
        } else {
            /* read what is available, feed the parser, maybe write */
        }
    }
}
```

### Per-connection state

Because you jump between clients, each connection needs a struct that remembers where it was:

```c
struct conn {
    int              fd;
    char             in[8192];
    size_t           in_len;
    enum parse_state state;
    const char      *out;      /* bytes still to send */
    size_t           out_len;
};
```

Store a pointer to it in `ev.data.ptr`. This is exactly why the resumable state-machine parser matters here.

### Level- vs edge-triggered

- **Level-triggered** (default): epoll keeps reporting a fd while data is waiting. Forgiving.
- **Edge-triggered** (`EPOLLET`): reports only when *new* data arrives. You must read until `EAGAIN` every time or you will miss data. Faster but easier to get wrong. Start with level-triggered.

### Check yourself

- Why do you only register for `EPOLLOUT` when you have output pending?
- What happens to every other client if one handler does a slow, blocking `read` of a file?

---

## Resource ownership

Servers run for days. A small leak per request becomes a crash.

- **Every `accept` needs exactly one `close`.** Leaked fds eventually make `accept` fail with `EMFILE`.
- **Decide who owns each buffer.** In the slice design above, the connection owns the buffer and the request only borrows from it.
- **Arena allocation:** allocate per-request memory from one block and free it all at once when the request ends. It turns many `free` calls into one and makes leaks almost impossible.
- **Single exit path:** `goto cleanup` at the end of a handler (chapter 3) keeps cleanup in one place.

### Check yourself

- Run your server under `valgrind` while sending 1000 requests. Does memory grow?
- How many fds can your process open? (`ulimit -n`)

---

## Security mindset

Assume every client is hostile.

- **Buffer overflows:** every copy needs a bound. Prefer `snprintf`, explicit lengths and slices over `strcpy`/`sprintf`.
- **Path traversal:** canonicalize with `realpath` and check the prefix.
- **Resource exhaustion:** limits on header size, body size, header count, connections, and timeouts on every phase.
- **Integer overflow:** `Content-Length: 18446744073709551615` must not wrap into a tiny allocation.
- **Request smuggling:** reject requests that have both `Content-Length` and `Transfer-Encoding`, or conflicting `Content-Length` values.
- **Privileges:** never run your learning server as root.

Build and test with sanitizers on all the time:

```bash
gcc -g -O1 -fsanitize=address,undefined -Wall -Wextra -o server *.c
```

---

## Tools for testing and debugging

| Tool | Use it for |
|------|-----------|
| `curl -v http://localhost:8080/` | see the exact request and response headers |
| `nc localhost 8080` | type raw HTTP by hand and send broken requests |
| `printf 'GET / HTTP/1.1\r\nHost: x\r\n\r\n' \| nc localhost 8080` | scripted raw requests |
| Your web browser + dev tools Network tab | real-world client behaviour |
| `wrk`, `ab`, `oha` | load testing and finding concurrency bugs |
| `strace -f ./server` | watch every system call your server makes |
| `ss -tlnp` | list listening sockets and connection states |
| `tcpdump` / Wireshark | see packets on the wire |
| `valgrind --leak-check=full` | memory leaks and invalid accesses |
| `gdb` | step through a crash |

Send malformed input on purpose: missing `Host`, a 20 KB header, a request with only `\n` line endings, a body shorter than its `Content-Length`. Your server should reply with an error or close, never crash.

---

## Milestones

Each milestone has a goal and a test that proves it works. How you get there is up to you.

| # | Goal | Done when |
|---|------|-----------|
| 1 | Echo server | `nc` connects, what you type comes back, server survives the client leaving |
| 2 | Hard-coded HTTP | `curl` and a browser both show a fixed "Hello" page |
| 3 | Real parsing | method, path and headers are printed for every request; malformed input gets 400 |
| 4 | Static files | a small website with HTML, CSS and images loads in the browser, with correct MIME types |
| 5 | Safety | `curl --path-as-is http://localhost:8080/../../etc/passwd` is refused; sanitizers stay quiet under fuzzed input |
| 6 | Concurrency | a slow `nc` connection that sends nothing does not block `curl` |
| 7 | Keep-alive | `curl -v` with two URLs on one command line reuses the connection |
| 8 | Event loop | rewrite milestone 6 with `epoll`; `wrk -c 1000` runs without errors |
| 9 | Your choice | routing table, logging, config file, `POST` handling, or directory listings |

After each milestone, write down one thing that surprised you. Those notes become your own reference.

---

## Going further

- **Routing:** map method + path patterns to handler functions using a table of function pointers (chapter 5).
- **Dynamic content:** run external programs CGI-style with `fork`, `pipe`, `dup2` and `exec`.
- **Chunked encoding:** both reading chunked request bodies and streaming responses.
- **Caching:** `Last-Modified`, `If-Modified-Since`, `ETag`, `304 Not Modified`.
- **Range requests:** `Range: bytes=0-1023` and `206 Partial Content`, needed for video seeking.
- **Compression:** `Accept-Encoding: gzip` with zlib.
- **TLS:** HTTPS through OpenSSL or a smaller library; a big topic on its own.
- **HTTP/2:** binary framing, multiplexed streams and header compression. Read about it once HTTP/1.1 feels easy.

---

## References

- **RFC 9110**, *HTTP Semantics*: methods, status codes, headers.
- **RFC 9112**, *HTTP/1.1*: message syntax, framing, connection management. This is the one your parser follows.
- **RFC 3986**, *URI Generic Syntax*: paths and percent-encoding.
- **Beej's Guide to Network Programming**: a free and friendly introduction to sockets in C.
- **man pages** (run `man 2 socket`, for example): `socket(2)`, `bind(2)`, `listen(2)`, `accept(2)`, `getaddrinfo(3)`, `socket(7)`, `tcp(7)`, `epoll(7)`, `sendfile(2)`, `pthreads(7)`.
- ***The Linux Programming Interface*** by Michael Kerrisk: the deep reference for everything in chapter 8 and this guide.
- ***UNIX Network Programming*** by W. Richard Stevens: the classic book on sockets and server design.

