#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>

void handle_error(const char *msg)
{
  perror(msg);
  exit(1);
}

char read_pipe(int *p)
{
  char c;
  c = 0;
  if (read(p[0], &c, 1) < 0) {
    handle_error("read_pipe");
  }
  return c;
}

char write_pipe(int *p, char c)
{
  if (write(p[1], &c, 1) < 0) {
    handle_error("write_pipe");
  }
}
  
int socket_connect(const struct sockaddr *saddr, socklen_t addrlen)
{
  int client = socket(saddr->sa_family, SOCK_STREAM, 0);
  if (client < 0) {
    handle_error("client socket");
  }

  // client is a blocking socket. As a side note, the fact that this call
  // returns proves that connect() does not wait for accept().
  if (connect(client, (struct sockaddr *) saddr, addrlen) < 0) {
    handle_error("connect");
  }

  return client;
}

template <typename T> void expect_equal(T a, T b, const char *msg)
{
  if (! (a == b)) {
    if (msg != NULL) {
      std::cout << msg << ": ";
    }
    std::cout << "expected " << a << ", got " << b;
    std::cout << std::endl;
  }
}

void usage()
{
  std::cerr << "Usage: epoll-test [unix|inet] [path|port]" << std::endl;
  exit(1);
}

int main(int argc, const char * const argv[])
{
  if (argc != 3) {
    usage();
  }

  const char *protocol = argv[1];
  const char *port = argv[2];

  struct sockaddr_un saddr_un;
  struct sockaddr_in saddr_in;
  struct sockaddr *saddr;
  socklen_t addrlen;

  if (strcmp(protocol, "unix") == 0) {
    unlink(port);
    memset(&saddr_un, 0, sizeof(saddr_un));
    saddr_un.sun_family = AF_UNIX;
    strncpy(saddr_un.sun_path, port, sizeof(saddr_un.sun_path)-1);
    saddr = (struct sockaddr *) &saddr_un;
    addrlen = sizeof(saddr_un);
  }
  else if (strcmp(protocol, "inet") == 0) {
    memset(&saddr_in, 0, sizeof(saddr_in));
    saddr_in.sin_family = AF_INET;
    saddr_in.sin_port = htons(atoi(port));
    if (inet_aton("127.0.0.1", &saddr_in.sin_addr) < 0) {
      handle_error("inet_aton");
    }
    saddr = (struct sockaddr *) &saddr_in;
    addrlen = sizeof(saddr_in);
  }
  else {
    usage();
  }

  // Parent is "server", child is "client". Set up server-to-client and
  // client-to-server pipes for synchronization.
  int s2c[2], c2s[2];
  if (pipe(s2c) < 0 || pipe(c2s) < 0) {
    handle_error("pipe");
  }

  int pid = fork();
  switch (pid) {
  case -1:
    handle_error("fork");
    break;

  case 0:
    // Child.
    // - On 'c', connect to socket and reply 'd'.
    // - On 'q', reply 'r' and exit.
    // - Otherwise, fail.
    char c;
    while ((c = read_pipe(s2c))) {
      switch (c) {
      case 'c':
	socket_connect(saddr, addrlen);
	write_pipe(c2s, 'd');
	break;
      case 'q':
	write_pipe(c2s, 'r');
	exit(0);
      default:
	break;
      }
    }
    exit(1);
    break;

  default:
    // Create a listening socket on sockaddr_un.
    int listener = socket(saddr->sa_family, SOCK_STREAM, 0);
    if (listener < 0) {
      handle_error("socket");
    }
    if (bind(listener, (struct sockaddr *) saddr, sizeof(*saddr)) < 0) {
      handle_error("bind");
    }
    if (listen(listener, 1024) < 0) {
      handle_error("listen");
    }

    // Set up epoll with edge triggering on listener.
    struct epoll_event events[16];
    const int epollfd = epoll_create(1024);
    struct epoll_event event = {
      EPOLLIN | /* EPOLLOUT | EPOLLRDHUP | EPOLLPRI | */EPOLLET, 0
    };
    event.data.fd = listener;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, listener, &event);

    int timeout = 0;
    
    // Before connect, nothing.
    int ret;
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll before connect");

    // Ask client to connect.
#ifdef INLINE_CONNECT
    socket_connect(saddr, addrlen);
#else
    write_pipe(s2c, 'c');
    expect_equal('d', read_pipe(c2s), "client connect 1 confirmation");
#endif

    // The connection triggers EPOLLIN.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(1, ret, "epoll 1 after connect 1");

    // A second epoll does not return EPOLLIN due to EPOLLET.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll 2 after connect 1");

    // Ask the client to connect again.
#ifdef INLINE_CONNECT
    socket_connect(saddr, addrlen);
#else
    write_pipe(s2c, 'c');
    expect_equal('d', read_pipe(c2s), "client connect 2 confirmation");
#endif

    // CONFUSION. We have NOT called accept, so the client's first connection
    // should still be pending. The socket has not changed from "not empty" so
    // I would not expect another EPOLLIN event. But it arrives, as
    // expect_equal displays a warning about it.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll 1 after connect 2");

    // As expected, EPOLLET means we do not get an event here.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll 2 after connect 2");

    // Accept one connection.
    int sock = accept(listener, NULL, 0);
    if (sock < 0) {
      handle_error("accept");
    }
	
    // The queue is still not empty, so still no EPOLLIN after accept.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll 3 after accept");

    // What if we close the socket?
    if (close(sock) < 0) {
      handle_error("accept");
    }
      
    // The queue is still not empty, so still no EPOLLIN.
    ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout);
    expect_equal(0, ret, "epoll 4 after close");

    // Tell the child to quit.
    write_pipe(s2c, 'q');
    expect_equal('r', read_pipe(c2s), "client quit confirmation");
  }
  
  return 0;
}
