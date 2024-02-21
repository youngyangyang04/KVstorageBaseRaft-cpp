#include <arpa/inet.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <stack>
#include "monsoon.h"

static int listen_sock = -1;

void test_accept();

// task
void watch_io_read() { monsoon::IOManager::GetThis()->addEvent(listen_sock, monsoon::READ, test_accept); }

void test_accept() {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  socklen_t len = sizeof(addr);
  int fd = accept(listen_sock, (struct sockaddr *)&addr, &len);
  if (fd < 0) {
    std::cout << "fd = " << fd << ",accept error" << std::endl;
  } else {
    std::cout << "fd = " << fd << ",accept success" << std::endl;
    fcntl(fd, F_SETFL, O_NONBLOCK);
    monsoon::IOManager::GetThis()->addEvent(fd, monsoon::READ, [fd]() {
      char buffer[1024];
      memset(buffer, 0, sizeof(buffer));
      while (true) {
        int ret = recv(fd, buffer, sizeof(buffer), 0);
        if (ret > 0) {
          std::cout << "client say: " << buffer << std::endl;
          ret = send(fd, buffer, ret, 0);
        }
        if (ret <= 0) {
          if (errno == EAGAIN) continue;
          close(fd);
          break;
        }
      }
    });
  }
  monsoon::IOManager::GetThis()->scheduler(watch_io_read);
}

void test_iomanager() {
  int port = 8080;
  struct sockaddr_in svr_addr;
  // socklen_t cli_len = sizeof(cli_addr);
  listen_sock = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_sock < 0) {
    std::cout << "creating listen socket error" << std::endl;
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  memset((char *)&svr_addr, 0, sizeof(svr_addr));
  svr_addr.sin_family = AF_INET;
  svr_addr.sin_port = htons(port);
  svr_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(listen_sock, (struct sockaddr *)&svr_addr, sizeof(svr_addr)) < 0) {
    std::cout << "bind error" << std::endl;
    return;
  }

  if (listen(listen_sock, 1024) < 0) {
    std::cout << "listen error" << std::endl;
    return;
  } else {
    std::cout << "listen success on port: " << port << std::endl;
  }

  fcntl(listen_sock, F_SETFL, O_NONBLOCK);

  monsoon::IOManager iomanager;
  iomanager.addEvent(listen_sock, monsoon::READ, test_accept);
}

int main(int argc, char *argv[]) {
  test_iomanager();
  return 0;
}
