#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "monsoon.h"

int sockfd;
void watch_io_read();

// 写事件回调，只执行一次，用于判断非阻塞套接字connect成功
void do_io_write() {
  std::cout << "write callback" << std::endl;
  int so_err;
  socklen_t len = size_t(so_err);
  getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &so_err, &len);
  if (so_err) {
    std::cout << "connect fail" << std::endl;
    return;
  }
  std::cout << "connect success" << std::endl;
}

// 读事件回调，每次读取之后如果套接字未关闭，需要重新添加
void do_io_read() {
  std::cout << "read callback" << std::endl;
  char buf[1024] = {0};
  int readlen = 0;
  readlen = read(sockfd, buf, sizeof(buf));
  if (readlen > 0) {
    buf[readlen] = '\0';
    std::cout << "read " << readlen << " bytes, read: " << buf << std::endl;
  } else if (readlen == 0) {
    std::cout << "peer closed";
    close(sockfd);
    return;
  } else {
    std::cout << "err, errno=" << errno << ", errstr=" << strerror(errno) << std::endl;
    close(sockfd);
    return;
  }
  // read之后重新添加读事件回调，这里不能直接调用addEvent，因为在当前位置fd的读事件上下文还是有效的，直接调用addEvent相当于重复添加相同事件
  monsoon::IOManager::GetThis()->scheduler(watch_io_read);
}

void watch_io_read() {
  std::cout << "watch_io_read" << std::endl;
  monsoon::IOManager::GetThis()->addEvent(sockfd, monsoon::READ, do_io_read);
}

void test_io() {
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  monsoon::CondPanic(sockfd > 0, "scoket should >0");
  fcntl(sockfd, F_SETFL, O_NONBLOCK);

  sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons(80);
  inet_pton(AF_INET, "36.152.44.96", &servaddr.sin_addr.s_addr);

  int rt = connect(sockfd, (const sockaddr *)&servaddr, sizeof(servaddr));
  if (rt != 0) {
    if (errno == EINPROGRESS) {
      std::cout << "EINPROGRESS" << std::endl;
      // 注册写事件回调，只用于判断connect是否成功
      // 非阻塞的TCP套接字connect一般无法立即建立连接，要通过套接字可写来判断connect是否已经成功
      monsoon::IOManager::GetThis()->addEvent(sockfd, monsoon::WRITE, do_io_write);
      // 注册读事件回调，注意事件是一次性的
      monsoon::IOManager::GetThis()->addEvent(sockfd, monsoon::READ, do_io_read);
    } else {
      std::cout << "connect error, errno:" << errno << ", errstr:" << strerror(errno) << std::endl;
    }
  } else {
    std::cout << "else, errno:" << errno << ", errstr:" << strerror(errno) << std::endl;
  }
}

void test_iomanager() {
  monsoon::IOManager iom;
  // monsoon::IOManager iom(10); // 演示多线程下IO协程在不同线程之间切换
  iom.scheduler(test_io);
}

int main(int argc, char *argv[]) {
  test_iomanager();

  return 0;
}