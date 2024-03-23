// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Util.h"

#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


const int MAX_BUFF = 4096;
ssize_t readn(int fd, void *buff, size_t n) {
  size_t nleft = n;
  ssize_t nread = 0;
  ssize_t readSum = 0;
  char *ptr = (char *)buff;
  while (nleft > 0) {
    if ((nread = read(fd, ptr, nleft)) < 0) {
      if (errno == EINTR)
        nread = 0;
      else if (errno == EAGAIN) {
        return readSum;
      } else {
        return -1;
      }
    } else if (nread == 0)
      break;
    readSum += nread;
    nleft -= nread;
    ptr += nread;
  }
  return readSum;
}

ssize_t readn(int fd, std::string &inBuffer, bool &zero) {
  ssize_t nread = 0;
  ssize_t readSum = 0;
  while (true) {
    char buff[MAX_BUFF];
    if ((nread = read(fd, buff, MAX_BUFF)) < 0) {
      if (errno == EINTR) // 系统调用被一个信号中断
        continue;
      else if (errno == EAGAIN) { // 资源暂时不可用。通常在非阻塞 I/O 操作中会遇到这个错误码，表示当前操作无法立即完成，需要稍后再试
        return readSum;
      } else {
        perror("read error");
        return -1;
      }
    } else if (nread == 0) {
      // printf("redsum = %d\n", readSum);
      zero = true;
      break;
    }
    // printf("before inBuffer.size() = %d\n", inBuffer.size());
    // printf("nread = %d\n", nread);
    readSum += nread;
    // buff += nread;
    inBuffer += std::string(buff, buff + nread);
    // printf("after inBuffer.size() = %d\n", inBuffer.size());
  }
  return readSum;
}

ssize_t readn(int fd, std::string &inBuffer) {
  ssize_t nread = 0;
  ssize_t readSum = 0;
  while (true) {
    char buff[MAX_BUFF];
    if ((nread = read(fd, buff, MAX_BUFF)) < 0) {
      if (errno == EINTR)
        continue;
      else if (errno == EAGAIN) {
        return readSum;
      } else {
        perror("read error");
        return -1;
      }
    } else if (nread == 0) {
      // printf("redsum = %d\n", readSum);
      break;
    }
    // printf("before inBuffer.size() = %d\n", inBuffer.size());
    // printf("nread = %d\n", nread);
    readSum += nread;
    // buff += nread;
    inBuffer += std::string(buff, buff + nread);
    // printf("after inBuffer.size() = %d\n", inBuffer.size());
  }
  return readSum;
}

ssize_t writen(int fd, void *buff, size_t n) {
  size_t nleft = n;
  ssize_t nwritten = 0;
  ssize_t writeSum = 0;
  char *ptr = (char *)buff;
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0) {
        if (errno == EINTR) {
          nwritten = 0;
          continue;
        } else if (errno == EAGAIN) {
          return writeSum;
        } else
          return -1;
      }
    }
    writeSum += nwritten;
    nleft -= nwritten;
    ptr += nwritten;
  }
  return writeSum;
}

ssize_t writen(int fd, std::string &sbuff) {
  size_t nleft = sbuff.size();
  ssize_t nwritten = 0;
  ssize_t writeSum = 0;
  const char *ptr = sbuff.c_str();
  while (nleft > 0) {
    if ((nwritten = write(fd, ptr, nleft)) <= 0) {
      if (nwritten < 0) {
        if (errno == EINTR) {
          nwritten = 0;
          continue;
        } else if (errno == EAGAIN)
          break;
        else
          return -1;
      }
    }
    writeSum += nwritten;
    nleft -= nwritten;
    ptr += nwritten;
  }
  if (writeSum == static_cast<int>(sbuff.size()))
    sbuff.clear();
  else
    sbuff = sbuff.substr(writeSum);
  return writeSum;
}

// 用来处理 SIGPIPE 信号
/*
  在 Unix/Linux 系统中，当程序尝试向一个已关闭的套接字（socket）写入数据时，
  系统会产生 SIGPIPE 信号，该信号的默认行为是终止进程。
  为了避免进程因为 SIGPIPE 信号而意外终止，
  通常会将 SIGPIPE 信号的处理方式设置为忽略（SIG_IGN）。
  TODO 暂时意义不明。。。
*/
void handle_for_sigpipe() {
  struct sigaction sa;
  memset(&sa, '\0', sizeof(sa));
  sa.sa_handler = SIG_IGN;
  sa.sa_flags = 0;
  if (sigaction(SIGPIPE, &sa, NULL)) return;
}

// 设置套接字为非阻塞模式，使用非阻塞套接字可以更好地配合 epoll 模型
int setSocketNonBlocking(int fd) {
  int flag = fcntl(fd, F_GETFL, 0);
  if (flag == -1) return -1;

  flag |= O_NONBLOCK;
  if (fcntl(fd, F_SETFL, flag) == -1) return -1;
  return 0;
}

/*
  IPPROTO_TCP 表示 TCP 协议级别的选项。
  TCP_NODELAY 是一个选项名称，用于控制是否启用 Nagle 算法。
  设置套接字为禁用 Nagle 算法的模式，这意味着发送数据时无论数据包的大小如何都会立即发送，
  而不会等待 TCP 缓冲区填满或者等待 ACK 确认
*/
void setSocketNodelay(int fd) {
  int enable = 1;
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (void *)&enable, sizeof(enable));
}

// 设置套接字在关闭时的行为为延迟关闭，即调用 close() 函数关闭套接字时，会等待一段时间（30 秒），
// 如果在延迟时间内仍有未发送的数据，则会尝试继续发送数据
void setSocketNoLinger(int fd) {
  struct linger linger_;
  linger_.l_onoff = 1;  // 打开 SO_LINGER 选项，即启用套接字关闭时的延迟关闭
  linger_.l_linger = 30;  // 延迟关闭的时间，单位为秒
  setsockopt(fd, SOL_SOCKET, SO_LINGER, (const char *)&linger_,
             sizeof(linger_));
}

void shutDownWR(int fd) {
  shutdown(fd, SHUT_WR);
  // printf("shutdown\n");
}

// 创建并绑定一个socket到对应端口上，并且设置其开始监听，返回该socket的fd
int socket_bind_listen(int port) {
  // 检查port值，取正确区间范围
  if (port < 0 || port > 65535) return -1;

  // 创建socket(IPv4 + TCP)，返回监听描述符
  int listen_fd = 0;
  /*
    创建了一个基于IPv4的TCP套接字。
    AF_INET 表示使用IPv4地址族
    SOCK_STREAM 表示创建一个面向连接的流式套接字（TCP）
    0表示使用默认协议（通常为TCP/IP协议栈）
  */
  if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) return -1;

  int optval = 1;
  /*
    用于设置【地址重用】选项，消除bind时"Address already in use"错误
    SO_REUSEADDR 表示地址重用。当设置了这个选项后，允许在同一端口上启动同一服务器的多个实例，
      即使之前的实例仍在运行。这对于服务器进程意外终止后快速重新启动而不需要等待一段时间以释放
      套接字是很有用的。
  */
  if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &optval,
                 sizeof(optval)) == -1) {
    close(listen_fd);
    return -1;
  }

  // 设置服务器IP和Port，和监听描述副绑定
  struct sockaddr_in server_addr;
  bzero((char *)&server_addr, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  // 表示将套接字绑定到本地所有网络接口上的任意IP地址。htonl() 函数用于将主机字节序转换为网络字节序
  // INADDR_ANY: 通配地址
  server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  server_addr.sin_port = htons((unsigned short)port);
  if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) ==
      -1) {
    close(listen_fd);
    return -1;
  }

  // 开始监听，最大等待队列长为LISTENQ
  // listen用于将套接字设置为监听状态，并指定连接队列的最大长度，不会阻塞
  if (listen(listen_fd, 2048) == -1) {
    close(listen_fd);
    return -1;
  }

  // 无效监听描述符
  if (listen_fd == -1) {
    close(listen_fd);
    return -1;
  }
  return listen_fd;
}