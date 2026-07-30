# tyson

> this README.md file is written by LLM.

An HTTP server implemented from scratch in C++20. Written as an educational
project to study socket programming and systems-level C++; not intended for
production use.

Current state: HTTP/1.1 request parsing and response generation. One request per
connection, answered and closed. Robustness (timeouts, keep-alive) and
concurrency are the next milestones.

## Requirements

- A C++20 compiler (GCC 11 or newer, Clang 14 or newer)
- CMake 3.16 or newer
- Linux (POSIX sockets; developed on Ubuntu)

## Build

```bash
./run.sh
```

`run.sh` configures `build/`, compiles with `-j$(nproc)`, refreshes the
`compile_commands.json` symlink for clangd, and then execs the server. To take
the steps one at a time:

```bash
cmake -S . -B build
cmake --build build
./build/server
```

The server listens on `0.0.0.0:8080`, IPv4 only, and handles one connection at
a time.

## Usage

```bash
curl -v http://localhost:8080/
```

A browser at `http://localhost:8080/` renders the page. On the wire the
response is:

```
HTTP/1.1 200 OK\r\n
Content-Type: text/html; charset=utf-8\r\n
Content-Length: 133\r\n
Connection: close\r\n
\r\n
<!doctype html>
<html><head><title>tyson</title></head>
<body><h1>tyson</h1><p>Handwritten HTTP, stage 2 complete.</p></body></html>
```

`Content-Length` and `Connection: close` are not written by the code that builds
the response — `serialize()` appends both, computing the length from
`body.size()`. A caller cannot get the framing wrong by hand.

What the routes answer:

| Request                           | Response                               |
| --------------------------------- | -------------------------------------- |
| `GET /`                           | `200` and the HTML page                |
| `GET` anything else               | `404 Not Found`                        |
| `POST /`, `HEAD /`                | `405 Method Not Allowed`, `Allow: GET` |
| `FROB /`, `DELETE /`, `get /`     | `501 Not Implemented`                  |
| grammar violation                 | `400 Bad Request`                      |
| head over `kMaxHeadSize`          | `431 Request Header Fields Too Large`  |
| declared body over `kMaxBodySize` | `413 Content Too Large`                |

A session on the server side:

```
listening on 0.0.0.0:8080 (fd 3)
connection from 127.0.0.1:51052 (fd 4)
 GET /
 responded 200
connection closed
connection from 127.0.0.1:51054 (fd 4)
 GET /missing
 responded 404
connection closed
connection from 127.0.0.1:51062 (fd 4)
 malformed request -> 400
 responded 400
connection closed
```

Note the asymmetry in the last two cases: a malformed request still gets a
response, because the connection works fine and only the content was wrong. A
peer that vanishes gets nothing, because there is nobody left to talk to.
`is_disconnect()` is what tells those apart.

You can also speak HTTP by hand. `nc` sends a bare `\n` when you press Enter and
this parser requires CRLF, so drive it with `printf`:

```bash
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc localhost 8080
```

## How it works

Six syscalls, in a fixed order, and the framing loop that turns a boundary-less
byte stream into one HTTP message. This section is the part of the project worth
reading — the code is small, but almost none of the interesting behaviour is
visible in it.

### Startup

```
main()                                             apps/server.cpp
  |
  +-- ::signal(SIGPIPE, SIG_IGN)
  |     By default, writing to a connection whose peer is gone raises
  |     SIGPIPE and the process dies. Ignoring it turns that into a
  |     plain EPIPE return value we can inspect.
  |
  +-- make_listener(kDefaultPort)                  src/listener.cpp
        |
        |  ::socket(AF_INET, SOCK_STREAM, 0)  ->  fd 3
        |     AF_INET     IPv4 addresses
        |     SOCK_STREAM reliable, ordered byte stream, so: TCP
        |     0           default protocol for that pair (IPPROTO_TCP)
        |  state: active, no address of its own yet
        v
        |  ::setsockopt(fd 3, SO_REUSEADDR)
        |     Must be set before bind or it does nothing. Lets bind
        |     succeed while the previous run's address sits in TIME_WAIT
        |     (~60s on Linux) instead of failing with EADDRINUSE.
        v
        |  ::bind(fd 3, 0.0.0.0:8080)
        |     sin_addr is a local *address*; 0.0.0.0 (INADDR_ANY) is the
        |     wildcard that means "every local interface".
        |     htons/htonl are required: the wire is big-endian, x86 is not.
        |  state: bound
        v
        |  ::listen(fd 3, kBacklog = 16)
        |     The real change here is active -> PASSIVE. From this point
        |     the kernel completes TCP handshakes on its own, with no
        |     help from this process. The queue is a consequence of that.
        |  state: listening
        v
      returns fd 3, held by a FileDescriptor for the lifetime of the process
```

### The accept loop

```
  .------------------------- accept loop (forever) -------------------------.
  |                                                                         |
  |   ::accept(fd 3, &peer, &peer_len)                                      |
  |     BLOCKS while the accept queue is empty, which is most of the        |
  |     server's life. It never returns "nothing here".                     |
  |                    |                                                    |
  |          .---------+---------.                                          |
  |      returned < 0        returned >= 0                                  |
  |          |                    |                                         |
  |   EINTR or ECONNABORTED       |  a NEW descriptor, fd 4, plus the       |
  |     -> next iteration         |  peer's sockaddr_in. fd 3 is not        |
  |   anything else               |  consumed and keeps listening.          |
  |     -> die("accept")          v                                         |
  |                        handle_connection(fd 4)  ->  one exchange        |
  |                               |                                         |
  |                               v                                         |
  |                        ~FileDescriptor runs ::close(fd 4),              |
  |                        which sends FIN to the client                    |
  |                               |                                         |
  '-------------------------------+-----------------------------------------'
                  back to ::accept — one connection at a time
```

### Framing: why one read is not one request

TCP guarantees that bytes arrive reliably and in order. It guarantees nothing
about how they are _grouped_. The 85-byte request curl sends may arrive as one
read of 85, or 20 then 65, or one byte at a time — and a pipelining client's
next request may arrive glued onto the end of this one. So a request is complete
when its _content_ says it is complete, never when a `::read` returns.

HTTP/1.1 draws those boundaries two ways, and `request_reader.cpp` implements
exactly those two: the head ends at the four-byte sequence `\r\n\r\n`, and the
body ends after `Content-Length` bytes have been counted.

That is also why the buffer is a growable `std::string` rather than Stage 1's
fixed `char buf[4096]`: bytes must survive across reads. And why it is bounded —
without `kMaxHeadSize`, a client that sends headers forever grows the buffer
until the machine dies.

### Reading the head

```
  .--------------- read_head(fd 4) ------------- src/request_reader.cpp ----.
  |                                                                         |
  |   .-> search the buffer for CRLF CRLF                                   |
  |   |     The search runs BEFORE the next read, because a single read      |
  |   |     may already have delivered the terminator.                       |
  |   |       found  -> head_end = position, outcome ok, RETURN              |
  |   |                                                                      |
  |   |   buffer.size() > kMaxHeadSize (8 KiB)                               |
  |   |       -> head_too_large, RETURN                    (becomes a 431)   |
  |   |                                                                      |
  |   |   fill(fd, buffer): one ::read appended to the buffer                |
  |   |       n > 0    -> got_bytes                                          |
  |   |       n == 0   -> peer sent FIN mid-head. Two meanings:              |
  |   |                     buffer empty -> disconnected  (said nothing)     |
  |   |                     buffer has bytes -> malformed (half a request)   |
  |   |       EINTR    -> read again, same call                              |
  |   |       otherwise-> is_disconnect(errno) ? disconnected : io_error     |
  |   '---- got_bytes: search again                                          |
  '-------------------------------------------------------------------------'
```

On success the buffer holds the head, the `\r\n\r\n`, and possibly **leftover**
bytes — the start of the body, or a whole pipelined next request. Those bytes are
already consumed from the kernel; keeping them is not an optimization, it is
correctness. `head_end` is returned as an index into the same buffer rather than
splitting it, so both halves stay available to the caller.

### Parsing, then the body

```
read_request(fd 4)                              src/request_reader.cpp
  |
  +-- read_head(fd 4)          -> buffer + head_end, or an early outcome
  |
  +-- http::parse_head(buffer[0 .. head_end))   src/http.cpp — no syscalls
  |     |
  |     +-- parse_request_line("GET / HTTP/1.1")
  |     |     A gauntlet of rejections, then the assignments. Exactly two
  |     |     spaces; target must start with '/' (or be "*"); version must
  |     |     be HTTP/1.1 or HTTP/1.0. An unrecognized METHOD is NOT an
  |     |     error — it parses to Method::unknown and earns a 501, not a
  |     |     400. Methods are case-sensitive, so "get" is unknown too.
  |     |
  |     '-- parse_header_line("Host: localhost:8080"), once per CRLF line
  |           Name is everything before the first ':'; whitespace between
  |           name and colon is REJECTED (RFC 9112 §5.1 — two parsers
  |           disagreeing there is a request-smuggling ingredient). The
  |           value is trimmed of OWS (spaces and tabs) by moving a
  |           string_view's ends, copying nothing.
  |           Any unparseable line rejects the whole head. A parser that
  |           helpfully skipped them would ignore exactly what a smuggling
  |           attack needs it to ignore.
  |
  +-- find_header(request, "Content-Length")     case-insensitive, first match
  |     absent          -> no body, and that is fine for GET
  |     present         -> std::from_chars into std::size_t: strict decimal,
  |                        no sign, no leading space, no trailing junk, no
  |                        overflow. "42abc" and "-5" are malformed, not 42.
  |
  +-- content_length > kMaxBodySize (1 MiB)  ->  body_too_large  (413)
  |     Checked BEFORE reading a single body byte. The declaration is
  |     attacker-controlled; enforcing the limit after buffering would make
  |     the limit decorative.
  |
  +-- body = leftover bytes past CRLF CRLF, then fill() until
  |     body.size() >= content_length. FIN here means the request can never
  |     complete: malformed.
  |
  '-- body.resize(content_length)
        Trims two kinds of overshoot at once: fill() appends whole chunks,
        and a pipelined client's next request may already sit past the body.
        Dropping the surplus is correct only because we close after one
        response — under keep-alive those bytes ARE the next request.
```

### Answering

```
handle_connection(fd 4)                         src/connection.cpp
  |
  +-- switch (result.outcome)      no `default:` — -Wswitch is the checklist
  |     ok             -> route(request)
  |     malformed      -> 400        head_too_large -> 431
  |     body_too_large -> 413
  |     disconnected   -> RETURN, say nothing
  |     io_error       -> RETURN, log strerror(errno)
  |
  +-- route(request)                             src/http.cpp
  |     Method::unknown      -> 501
  |     any method but GET   -> 405 + "Allow: GET"  (RFC 9110 §15.5.6)
  |     target == "/"        -> 200 + the HTML page
  |     otherwise            -> 404
  |
  +-- http::serialize(response)  -> one std::string of wire bytes
  |     status line, then the caller's headers, then Content-Length from
  |     body.size(), then Connection: close, then THE blank line, then body.
  |
  '-- write_all(fd 4, wire.data(), wire.size())
        Loops internally: one ::write may accept fewer bytes than asked
        once the send buffer fills. On failure, is_disconnect(errno)
        separates "peer gone mid-response" from a real error.
```

### On the wire

Where the loops sit relative to the kernel is the thing the code hides:

```
   client                    kernel                       this process
      |                         |                               |
      |                         |                               |  blocked
      |---- SYN --------------->|                               |
      |<----------- SYN-ACK ----|                               |  handshake runs
      |---- ACK --------------->|                               |  in the kernel
      |  connect() returns      |                               |
      |                         |---- accept queue: fd 4 ------>|  wakes up
      |                         |                               |
      |-- "GET / HTTP/1.1\r\n" >|                               |
      |                         |---- recv buffer -------------->|  ::read -> 16
      |                         |                               |  no CRLFCRLF yet,
      |                         |                               |  read again
      |-- "Host: ...\r\n\r\n" ->|                               |
      |                         |---- recv buffer -------------->|  ::read -> 24
      |                         |                               |  CRLFCRLF found:
      |                         |                               |  parse, route,
      |                         |                               |  serialize
      |                         |<---- send buffer: 205 bytes ---|  write_all
      |<------ HTTP/1.1 200 ----|                               |
      |                         |<----------- ::close(fd 4) ----|  by RAII
      |<--------------- FIN ----|                               |
      |  curl sees the close     |                               |
      |  and does not reuse      |                               |  blocked again
      |  the connection          |                               |
```

The two reads above are the point of the whole framing layer. A server that
answered after the first one would reply to half a request.

## Build options

| Option             | Default | Effect                                          |
| ------------------ | ------- | ----------------------------------------------- |
| `TYSON_SANITIZE`   | `ON`    | AddressSanitizer and UndefinedBehaviorSanitizer |
| `CMAKE_BUILD_TYPE` | `Debug` | Standard CMake build type                       |

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DTYSON_SANITIZE=OFF
```

## Layout

```
include/tyson/    public headers
src/              implementation
apps/server.cpp   entry point and accept loop
```

| Module            | Responsibility                                                                               |
| ----------------- | -------------------------------------------------------------------------------------------- |
| `config`          | Compile-time constants: port, backlog, buffer size, head/body caps                           |
| `error`           | `die()` for fatal faults, `is_disconnect()` to classify routine ones                         |
| `file_descriptor` | RAII descriptor ownership; move-only. Header-only                                            |
| `io`              | `write_all()`, which loops over partial writes                                               |
| `listener`        | `make_listener()`: socket, `SO_REUSEADDR`, bind, listen                                      |
| `http`            | `Request`/`Response`, parsing, `serialize()`, `route()`. No syscalls                         |
| `request_reader`  | `read_head()`/`read_request()`: the framing layer. The only Stage-2 file that calls `::read` |
| `connection`      | `handle_connection()` and peer address formatting                                            |

Dependency direction — a module may only depend on modules to its right:

```
connection -> request_reader -> http -> (nothing but the standard library)
     \              \
      io, error      config, error
```

Sockets never appear inside the parser, and parsing never appears inside the
socket loop. `src/http.cpp` contains no `::read`, `::write`, or `::close`, which
is what will let the parser be unit-tested with plain strings and no network.
Routing lives in `http` alongside serialization, so `connection` is left holding
only the socket lifecycle.

Two limits guard attacker-controlled sizes, both in `config.hpp`:

| Constant       | Value | Enforced by                               |
| -------------- | ----- | ----------------------------------------- |
| `kMaxHeadSize` | 8 KiB | `read_head()`, before the next `::read`   |
| `kMaxBodySize` | 1 MiB | `read_request()`, before reading the body |

8 KiB matches the Apache and nginx defaults; real browsers send well under 2 KiB
of headers.

## Conventions

Filenames `snake_case.{hpp,cpp}`; types `PascalCase`; functions and variables
`snake_case`; private data members carry a trailing underscore; compile-time
constants are prefixed `k`.

Headers declare, sources define. No source file includes another source file.
Every header begins with `#pragma once`. All symbols are declared in
`namespace tyson`; the HTTP module nests one deeper as `namespace tyson::http`,
so call sites read as documentation (`http::Request`, `http::serialize`).
File-private helpers live in an anonymous namespace inside the `.cpp`.

`std::string_view` is used for function parameters and transient locals during
parsing; `std::string` for anything stored in a struct that outlives the parse.
That is why `Request::target` is a `std::string` even though the parser sliced it
out as a view — storing it copies it into safety.

Includes are written relative to the project root, for example
`#include "tyson/io.hpp"`, never as a relative path.

Formatting is applied by hand. There is no `.clang-format` in the repository
yet, so nothing enforces the layout — adding one is on the list.

## Diagnostics

```bash
ss -tlnp | grep 8080              # confirm the listening socket and its backlog
strace -e trace=read ./build/server   # watch the framing loop read in pieces
ls /proc/$(pgrep -x server)/fd    # inspect open descriptors
```

A descriptor number that increases across successive connections indicates a
missing `close()`. It should come back to the same number for every client,
because the previous one was released. (The absolute value is not fixed — a
sanitized build may hold descriptors of its own.)

To prove the framing layer earns its keep, send a request one byte at a time and
watch `strace` report dozens of `read(4, ...) = 1` before a single response:

```python
import socket, time
s = socket.create_connection(("127.0.0.1", 8080))
request = b"GET / HTTP/1.1\r\nHost: localhost\r\n\r\n"
for i in range(len(request)):
    s.sendall(request[i:i+1])
    time.sleep(0.05)
print(s.recv(4096).decode(errors="replace"))
```

And to see leftover bytes, send two pipelined requests in one write — the buffer
ends up larger than `head_end + 4`:

```bash
printf 'GET / HTTP/1.1\r\nHost: a\r\n\r\nGET /second HTTP/1.1\r\n\r\n' | nc localhost 8080
```

curl reports `Connection refused` for `[::1]:8080` before falling back to
`127.0.0.1`, because the listener is IPv4 only.

`std::cout` is fully buffered when stdout is not a terminal, so the startup
banner and per-request lines will not appear in a redirected log until the buffer
fills or the process exits. Watch it on a terminal, or run it under
`script -qc './build/server'`, when you need those lines as they happen.

## Known limitations

Expected at this stage, and each one is a roadmap item rather than a surprise.

Connection handling:

- One connection at a time. A second client's `connect()` succeeds immediately
  — the kernel does that — but it is not read from until the first client is
  finished.
- No timeouts anywhere, and this is now the sharpest edge. A client that
  declares `Content-Length: 100` and sends nine bytes leaves the server blocked
  in `::read` forever, and because connections are serial the whole server is
  hostage to it:
  ```bash
  printf 'POST / HTTP/1.1\r\nContent-Length: 100\r\n\r\nonly-this' | nc localhost 8080
  ```
- No keep-alive. Every response carries `Connection: close`, one exchange per
  connection, and leftover bytes past the declared body are dropped. Under
  keep-alive those bytes would be the next request.
- A `431` or `413` stops reading early, so the descriptor is closed with unread
  data still in the receive buffer. Linux answers that with RST rather than FIN,
  which can flush the client's receive queue and lose the response that was
  already delivered — some clients print the status, some print nothing.

Protocol coverage:

- Strict CRLF only. A request with bare `\n` line endings never completes a head
  and the connection simply waits. RFC 9112 §2.2 permits recognizing bare LF;
  the honest fix for a client that never finishes is a read timeout, not looser
  parsing.
- Duplicate `Content-Length` headers are not rejected. Both are preserved in the
  header vector, but `find_header` returns the first and the second is ignored —
  RFC 9112 §6.3 requires a 400 when they conflict.
- `Transfer-Encoding: chunked` is neither implemented nor refused; such a request
  is treated as having no body and the chunk framing is left unread.
- `kMaxHeadSize` is soft by up to `kBufferSize`. The size check runs before each
  read and `fill()` appends up to 4096 bytes at a time, so a head of roughly
  9 KiB can still be accepted.
- Only `GET`, `HEAD`, and `POST` are recognized as methods, so `DELETE` and
  friends answer `501` rather than `405`. `HEAD` is recognized but not
  implemented, and answers `405`.
- The target is stored verbatim: no percent-decoding, no query parsing, no path
  normalization. Absolute-form targets (`http://host/path`, meant for proxies)
  are rejected.
- The `Host` header is not required, though HTTP/1.1 mandates it.
- IPv4 only, and no graceful shutdown: the process is stopped with a signal.

## Roadmap

- [x] Toolchain: CMake, GDB, sanitizers
- [x] TCP echo server
- [x] HTTP/1.1 request and response parsing (RFC 9112)
- [ ] Robustness: timeouts, keep-alive, partial requests
- [ ] Concurrency: `fork`, threads, `poll`, `epoll`
