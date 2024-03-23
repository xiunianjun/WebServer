// @Author Lin Ya
// @Email xxbbb@vip.qq.com
#include "Server.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <functional>
#include "Util.h"
#include "base/Logging.h"

/*
  1. 创建监听socket，及其对应的channel对象
  2. 忽略SIGPIPE信号
*/
Server::Server(EventLoop *loop, int threadNum, int port)
    : loop_(loop),
      threadNum_(threadNum),
      eventLoopThreadPool_(new EventLoopThreadPool(loop_, threadNum)),
      started_(false),
      acceptChannel_(new Channel(loop_)),
      port_(port),  // Main中传来的，也即端口比如说60000
      listenFd_(socket_bind_listen(port_)) {  // 此处创建了一个监听的socket
  acceptChannel_->setFd(listenFd_);
  handle_for_sigpipe();
  if (setSocketNonBlocking(listenFd_) < 0) {
    perror("set socket non block failed");
    abort();
  }
}

void Server::start() {
  // 值得注意的是，这些子线程的loop并不是loop_，似乎是它里面新建的一个空loop，可能在后面会add poller
  eventLoopThreadPool_->start();
  // acceptChannel_->setEvents(EPOLLIN | EPOLLET | EPOLLONESHOT);
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
  // 对于监听线程来说，有新的连接来到->EPOLLIN->read handler被触发，所以我们将handNewConn绑定到read handler上
  acceptChannel_->setReadHandler(bind(&Server::handNewConn, this));
  acceptChannel_->setConnHandler(bind(&Server::handThisConn, this));
  // 自此以来，main loop就管理accept了
  loop_->addToPoller(acceptChannel_, 0);
  started_ = true;
}

void Server::handNewConn() {
  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(struct sockaddr_in));
  socklen_t client_addr_len = sizeof(client_addr);
  int accept_fd = 0;
  // 当监听套接字上触发 EPOLLIN 事件时，表示有新的连接请求到达
  // 在这种情况下，通常应该调用 accept() 函数来接受新的连接
  while ((accept_fd = accept(listenFd_, (struct sockaddr *)&client_addr,
                             &client_addr_len)) > 0) {
    EventLoop *loop = eventLoopThreadPool_->getNextLoop();
    LOG << "New connection from " << inet_ntoa(client_addr.sin_addr) << ":"
        << ntohs(client_addr.sin_port);
    // cout << "new connection" << endl;
    // cout << inet_ntoa(client_addr.sin_addr) << endl;
    // cout << ntohs(client_addr.sin_port) << endl;
    /*
    // TCP的保活机制默认是关闭的
    int optval = 0;
    socklen_t len_optval = 4;
    getsockopt(accept_fd, SOL_SOCKET,  SO_KEEPALIVE, &optval, &len_optval);
    cout << "optval ==" << optval << endl;
    */
    // 限制服务器的最大并发连接数
    if (accept_fd >= MAXFDS) {
      close(accept_fd);
      continue;
    }
    // 设为非阻塞模式
    if (setSocketNonBlocking(accept_fd) < 0) {
      LOG << "Set non block failed!";
      // perror("Set non block failed!");
      return;
    }

    // 建立连接的ACK确实还是得尽快回复
    setSocketNodelay(accept_fd);
    // setSocketNoLinger(accept_fd);

    shared_ptr<HttpData> req_info(new HttpData(loop, accept_fd));
    req_info->getChannel()->setHolder(req_info);
    // 这里相当于，把newEvent入队，然后再唤醒工作线程
    loop->queueInLoop(std::bind(&HttpData::newEvent, req_info));
  }
  // 注册下一次accept事件
  acceptChannel_->setEvents(EPOLLIN | EPOLLET);
}