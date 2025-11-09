// Build (Windows): cl /std:c++17 /O2 server_dbg_portable.cpp /link Ws2_32.lib
// Build (MinGW):   g++ -std=c++17 -O2 server_dbg_portable.cpp -lws2_32 -o server.exe
// Build (Linux/macOS): g++ -std=c++17 -O2 server_dbg_portable.cpp -o server

#include <string>
#include <iostream>
#include <vector>
#include <cstdlib>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
  using socket_t = SOCKET;
  static bool sockets_init(){ WSADATA w; return WSAStartup(MAKEWORD(2,2), &w)==0; }
  static void sockets_cleanup(){ WSACleanup(); }
  static void socket_close(socket_t s){ closesocket(s); }
  static bool socket_invalid(socket_t s){ return s==INVALID_SOCKET; }
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <unistd.h>
  using socket_t = int;
  static bool sockets_init(){ return true; }
  static void sockets_cleanup(){}
  static void socket_close(socket_t s){ close(s); }
  static bool socket_invalid(socket_t s){ return s<0; }
#endif

static void send_line(socket_t s, const std::string& ascii_line){
  std::string x = ascii_line + "\n"; // ensure \n for Java readLine()
#ifdef _WIN32
  send(s, x.c_str(), (int)x.size(), 0);
#else
  send(s, x.c_str(), x.size(), 0);
#endif
}

int main(int argc, char** argv){
  int port = 8080;
  if(argc>1) port = std::atoi(argv[1]);

  if(!sockets_init()){ std::cerr<<"Socket init failed\n"; return 1; }

  socket_t srv = socket(AF_INET, SOCK_STREAM, 0);
  if(socket_invalid(srv)){ std::cerr<<"socket() failed\n"; sockets_cleanup(); return 1; }

#ifdef _WIN32
  BOOL yes = TRUE; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));
#else
  int yes = 1; setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
#endif

  sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_addr.s_addr=htonl(INADDR_ANY); addr.sin_port=htons((uint16_t)port);
  if(bind(srv,(sockaddr*)&addr,sizeof(addr))<0){ std::cerr<<"bind() failed\n"; socket_close(srv); sockets_cleanup(); return 1; }
  if(listen(srv,8)<0){ std::cerr<<"listen() failed\n"; socket_close(srv); sockets_cleanup(); return 1; }

  std::cout<<"Server listening on port "<<port<<" (ASCII, debug)\n";

  for(;;){
    sockaddr_in cli{};
#ifdef _WIN32
    int len = sizeof(cli);
#else
    socklen_t len = sizeof(cli);
#endif
    socket_t fd = accept(srv,(sockaddr*)&cli,&len);
    if(socket_invalid(fd)){ std::cerr<<"accept() failed\n"; continue; }
    std::cout<<"Client connected\n";

    std::vector<char> buf(1024);
    for(;;){
#ifdef _WIN32
      int n = recv(fd, buf.data(), (int)buf.size(), 0);
#else
      int n = (int)recv(fd, buf.data(), buf.size(), 0);
#endif
      if(n==0){ std::cout<<"Client closed connection\n"; break; }
      if(n<0){ std::cout<<"recv error\n"; break; }

      // Log raw bytes (show \r and \n explicitly)
      std::string payload; payload.reserve(n);
      for(int i=0;i<n;++i){
        char c = buf[i];
        if(c=='\r') payload += "\\r";
        else if(c=='\n') payload += "\\n";
        else if(c>=32 && c<=126) payload.push_back(c);
        else payload += ".";
      }
      std::cout<<"Received "<<n<<" bytes: ["<<payload<<"]\n";

      // Always answer with a full line so Java readLine() returns
      send_line(fd, "hello from server");
    }

    socket_close(fd);
    std::cout<<"Client disconnected\n";
  }

  socket_close(srv);
  sockets_cleanup();
  return 0;
}
