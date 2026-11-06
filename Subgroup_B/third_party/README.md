# Third-party code

| File | Library | Version | Licence | Used by |
|------|---------|---------|---------|---------|
| `httplib.h` | cpp-httplib (Yuji Hirose), header-only HTTP server and client | 0.65.0 (`CPPHTTPLIB_VERSION` in the header) | MIT (see the header's first lines) | `gui/server_main.cpp` and `gui/integrated/int_server_main.cpp` only |

Rules for this folder:
- Files here are not edited; they are replaced as a whole with a newer pinned release.
- Nothing is downloaded by the build or at run time.

On MinGW, the GUI builds with `-D_WIN32_WINNT=0x0A00` (Windows 10 or later, which the library requires) and links with `-lws2_32` (Windows sockets) and `-static` (see `gui/makefile`). No OpenSSL is used, so no TLS libraries are linked.
