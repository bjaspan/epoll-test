#include <stdio.h>
#include <stdlib.h>
#include <memory.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/epoll.h>
#include <sys/fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>


#define RETRY_EINTR_RC(rc, expr) \
    { \
        do \
        { \
            rc = (expr); \
        } while ((rc < 0) && (errno == EINTR)); \
    }

void handle_error(const char *msg)
{
  perror(msg);
  exit(1);
}

void usage()
{
  fprintf(stderr, "Usage: not sure yet\n");
}
    
int set_socket_nonblocking(int socket)
{
  int rc_nonblock;
  int flags = fcntl(socket, F_GETFL, 0);
  if (flags >= 0) {
    rc_nonblock = fcntl(socket, F_SETFL, flags | O_NONBLOCK);
  }
  else {
    rc_nonblock = flags;
  }

  return rc_nonblock;
}

int main(int argc, const char * const argv[])
{
  struct sockaddr_un saddr_un;

  if (argc < 1) {
    usage();
  }
  
  const char *path = argv[1];

  int s = socket(AF_UNIX, SOCK_STREAM, 0);
  if (s < 0) {
    handle_error("socket");
  }

  memset(&saddr_un, 0, sizeof(saddr_un));
  saddr_un.sun_family = AF_UNIX;
  strncpy(saddr_un.sun_path, path, sizeof(saddr_un.sun_path)-1);

  unlink(path);
  if (bind(s, (struct sockaddr *) &saddr_un, sizeof(saddr_un)) < 0) {
    handle_error("bind");
  }
  if (listen(s, 1024) < 0) {
    handle_error("listen");
  }

  if (set_socket_nonblocking(s) < 0) {
    handle_error("set_socket_nonblocking");
  }

  const int epollfd = epoll_create(1024);
  struct epoll_event event = {
    EPOLLIN | EPOLLOUT | EPOLLET, 0
  };
  event.data.fd = s;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, fds[1], &event);
  
  return 0;
}
