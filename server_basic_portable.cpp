// Build (Windows): cl /std:c++17 /O2 server_basic_portable.cpp /link Ws2_32.lib
// Execute (Windows): server_basic_portable.exe 8080
// Build (Linux/macOS): g++ -std=c++17 -O2 server_basic_portable.cpp -o server

#include <string>
#include <iostream>
#include <cstdlib>

// ------------ Cross-platform socket layer -----------------------------------
#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  static bool sockets_init() { WSADATA w; return WSAStartup(MAKEWORD(2,2), &w) == 0; }
  static void sockets_cleanup() { WSACleanup(); }
  static void socket_close(socket_t s) { closesocket(s); }
  static bool socket_is_invalid(socket_t s) { return s == INVALID_SOCKET; }
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <unistd.h>
  #include <errno.h>
  using socket_t = int;
  static bool sockets_init() { return true; }
  static void sockets_cleanup() {}
  static void socket_close(socket_t s) { close(s); }
  static bool socket_is_invalid(socket_t s) { return s < 0; }
#endif

// ------------ Line I/O (ASCII), one message per line ------------------------
static bool read_line(socket_t s, std::string& out) {
    out.clear();
    char buf[512];
    for (;;) {
#ifdef _WIN32
        int n = recv(s, buf, (int)sizeof(buf), 0);
#else
        int n = (int)recv(s, buf, sizeof(buf), 0);
#endif
        if (n == 0) return false;     // connection closed
        if (n < 0) return false;      // recv error
        for (int i = 0; i < n; ++i) {
            char c = buf[i];
            if (c == '\n') {          // end of message
                if (!out.empty() && out.back() == '\r') out.pop_back(); // handle CRLF
                return true;
            }
            out.push_back(c);
            if (out.size() > 8192) return false;  // safety guard
        }
    }
}

static void send_line(socket_t s, const std::string& ascii_line) {
    std::string x = ascii_line + "\n"; // line framing
#ifdef _WIN32
    send(s, x.c_str(), (int)x.size(), 0);
#else
    send(s, x.c_str(), x.size(), 0);
#endif
}

// ------------------------------ Main ----------------------------------------
int main(int argc, char** argv) {
    int port = 8080;
    if (argc > 1) port = std::atoi(argv[1]);

    if (!sockets_init()) {
        std::cerr << "Socket init failed\n";
        return 1;
    }

    socket_t srv = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_is_invalid(srv)) {
        std::cerr << "socket() failed\n";
        sockets_cleanup();
        return 1;
    }

    // Reuse address (nice-to-have)
#ifdef _WIN32
    BOOL yes = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#else
    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 0.0.0.0
    addr.sin_port = htons((uint16_t)port);

    if (bind(srv, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "bind() failed\n";
        socket_close(srv);
        sockets_cleanup();
        return 1;
    }
    if (listen(srv, 8) < 0) {
        std::cerr << "listen() failed\n";
        socket_close(srv);
        sockets_cleanup();
        return 1;
    }

    std::cout << "Server listening on port " << port << " (ASCII, one line per message)\n";

    for (;;) {
        sockaddr_in cli{};
#ifdef _WIN32
        int len = sizeof(cli);
#else
        socklen_t len = sizeof(cli);
#endif
        socket_t fd = accept(srv, (sockaddr*)&cli, &len);
        if (socket_is_invalid(fd)) {
            std::cerr << "accept() failed\n";
            continue;
        }

        std::cout << "Client connected\n";

        std::string line;
        while (read_line(fd, line)) {
            // Show what you received (proof of input reaching the server)
            std::cout << "Received: [" << line << "]\n";

            // Always reply one line so the Java client readLine() returns.
            send_line(fd, "hello from server");
        }

        socket_close(fd);
        std::cout << "Client disconnected\n";
    }

    socket_close(srv);
    sockets_cleanup();
    return 0;
}
