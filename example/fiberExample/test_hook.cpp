#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "monsoon.h"

const std::string LOG_HEAD = "[TASK] ";

void test_sleep() {
  std::cout << LOG_HEAD << "tid = " << monsoon::GetThreadId() << ",test_fiber_sleep begin" << std::endl;
  monsoon::IOManager iom(1, true);

  iom.scheduler([] {
    while (1) {
      sleep(6);
      std::cout << "task 1 sleep for 6s" << std::endl;
    }
  });

  iom.scheduler([] {
    while (1) {
      sleep(2);
      std::cout << "task2 sleep for 2s" << std::endl;
    }
  });

  std::cout << LOG_HEAD << "tid = " << monsoon::GetThreadId() << ",test_fiber_sleep finish" << std::endl;
}

void test_sock() {
  int sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(80);
  inet_pton(AF_INET, "36.152.44.96", &addr.sin_addr.s_addr);

  std::cout << "begin connect" << std::endl;
  int rt = connect(sock, (const sockaddr *)&addr, sizeof(addr));
  std::cout << "connect rt=" << rt << " errno=" << errno << std::endl;

  if (rt) {
    return;
  }

  const char data[] = "GET / HTTP/1.0\r\n\r\n";
  rt = send(sock, data, sizeof(data), 0);
  std::cout << "send rt=" << rt << " errno=" << errno << std::endl;

  if (rt <= 0) {
    return;
  }

  std::string buff;
  buff.resize(4096);

  rt = recv(sock, &buff[0], buff.size(), 0);
  std::cout << "recv rt=" << rt << " errno=" << errno << std::endl;

  if (rt <= 0) {
    return;
  }

  buff.resize(rt);
  std::cout << "--------------------------------" << std::endl;
  std::cout << buff << std::endl;
  std::cout << "--------------------------------" << std::endl;
}

int main() {
  // monsoon::IOManager iom;
  // iom.scheduler(test_sock);

  test_sleep();
}