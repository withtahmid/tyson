# tyson

An HTTP server implemented from scratch in C++20. Written as an educational
project to study socket programming and systems-level C++; not intended for
production use.

Current state: TCP echo server. HTTP/1.1 request parsing is the next milestone.

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
nc localhost 8080
```

Input is echoed back verbatim. A full session looks like this on the server
side:

```
listening on 0.0.0.0:8080 (fd 3)
connection from 127.0.0.1:59548 (fd 4)
 read 3 bytes
 peer closed (EOF)
connection closed
```

Neither way of ending the session should stop the server. Ctrl-D closes the
client's write side, so `::read` returns 0 and the connection is retired
cleanly. Ctrl-C kills `nc` and the kernel closes its descriptor on the way out,
which usually also arrives as a FIN; it becomes an RST when data is still
queued unread. The RST path shows up as `ECONNRESET` from `::read`, or `EPIPE`
from `::write`, and `is_disconnect()` classifies both as routine.

## How it works

Six syscalls, in a fixed order, and two nested loops. This section is the part
of the project worth reading — the code is small, but almost none of the
interesting behaviour is visible in it.

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
  |                        handle_connection(fd 4)  ->  echo loop below     |
  |                               |                                         |
  |                               v                                         |
  |                        ~FileDescriptor runs ::close(fd 4),              |
  |                        which sends FIN to the client                    |
  |                               |                                         |
  '-------------------------------+-----------------------------------------'
                  back to ::accept — one connection at a time
```

### The echo loop

```
  .-------------------------- echo loop (forever) --------------------------.
  |                                                                         |
  |   ::read(fd 4, buf, kBufferSize = 4096)                                 |
  |     BLOCKS until at least one byte arrives                              |
  |                    |                                                    |
  |     .--------------+--------------.--------------------------.          |
  |   n > 0                        n == 0                     n < 0         |
  |     |                            |                           |          |
  |     |                    peer sent FIN            errno EINTR           |
  |     |                    "peer closed (EOF)"        -> read again       |
  |     |                    -> return                is_disconnect()       |
  |     v                                               -> peer gone,       |
  |   write_all(fd 4, buf, n)                              return           |
  |     Loops internally, because one ::write may accept    otherwise       |
  |     fewer bytes than asked once the send buffer fills.  -> log, return  |
  |     |                                                                   |
  |     +-- all n bytes sent  -> read again                                 |
  |     '-- failed            -> is_disconnect(errno) ? peer gone : error,  |
  |                              return either way                          |
  '-------------------------------------------------------------------------'
```

### On the wire

Where the two loops sit relative to the kernel is the thing the code hides:

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
      |---- "hi\n" ------------>|                               |
      |                         |---- recv buffer: 3 bytes ---->|  ::read -> 3
      |                         |<---- send buffer: 3 bytes ----|  write_all
      |<------------ "hi\n" ----|                               |
      |                         |                               |
      |  Ctrl-D: close()        |                               |
      |---- FIN --------------->|                               |
      |                         |---- ::read returns 0 -------->|  EOF, return
      |                         |<----------- ::close(fd 4) ----|  by RAII
      |<--------------- FIN ----|                               |
      |                         |                               |  blocked again
```

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

| Module            | Responsibility                                                       |
| ----------------- | -------------------------------------------------------------------- |
| `config`          | Compile-time constants: port, backlog, buffer size                   |
| `error`           | `die()` for fatal faults, `is_disconnect()` to classify routine ones |
| `file_descriptor` | RAII descriptor ownership; move-only. Header-only                    |
| `io`              | `write_all()`, which loops over partial writes                       |
| `listener`        | `make_listener()`: socket, `SO_REUSEADDR`, bind, listen              |
| `connection`      | `handle_connection()` and peer address formatting                    |

## Conventions

Filenames `snake_case.{hpp,cpp}`; types `PascalCase`; functions and variables
`snake_case`; private data members carry a trailing underscore; compile-time
constants are prefixed `k`.

Headers declare, sources define. No source file includes another source file.
Every header begins with `#pragma once`. All symbols are declared in
`namespace tyson`.

Includes are written relative to the project root, for example
`#include "tyson/io.hpp"`, never as a relative path.

Formatting is applied by hand. There is no `.clang-format` in the repository
yet, so nothing enforces the layout — adding one is on the list.

## Diagnostics

```bash
ss -tlnp | grep 8080              # confirm the listening socket and its backlog
strace ./build/server             # trace every syscall in order
ls /proc/$(pgrep -x server)/fd    # inspect open descriptors
```

A descriptor number that increases across successive connections indicates a
missing `close()`. It should come back to the same number for every client,
because the previous one was released. (The absolute value is not fixed — a
sanitized build may hold descriptors of its own.)

`curl` is not a useful smoke test yet. It sends a complete HTTP request, the
echo server hands the request straight back, and curl rejects its own words as
a response:

```
> GET / HTTP/1.1
* Received HTTP/0.9 when not allowed
< GET / HTTP/1.1
* Invalid response header
```

It also tries `[::1]:8080` first and reports `Connection refused` before
falling back to `127.0.0.1`, because the listener is IPv4 only.

`std::cout` is fully buffered when stdout is not a terminal, so the startup
banner will not appear in a redirected log until the buffer fills or the process
exits. Watch it on a terminal, or run it under `script -qc './build/server'`,
when you need those lines as they happen.

## Known limitations

Expected at this stage, and each one is a roadmap item rather than a surprise:

- One connection at a time. A second client's `connect()` succeeds immediately
  — the kernel does that — but it is not read from until the first client is
  finished.
- No timeouts anywhere. A client that sends without ever reading fills the
  window, `write_all()` blocks in `::write`, and because connections are serial
  the server stops serving anyone while staying alive and healthy-looking.
- IPv4 only, and no graceful shutdown: the process is stopped with a signal.

## Roadmap

- [x] Toolchain: CMake, GDB, sanitizers
- [x] TCP echo server
- [ ] HTTP/1.1 request and response parsing (RFC 9112)
- [ ] Robustness: timeouts, keep-alive, partial requests
- [ ] Concurrency: `fork`, threads, `poll`, `epoll`
