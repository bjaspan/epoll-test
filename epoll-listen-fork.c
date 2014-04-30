/**
 * Create a listening socket, wait for connections using epoll with level- or
 * edge-triggering, and fork a child for each epoll event that sleeps for a
 * specified time then exits.
 */
 
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

void handle_error(const char *msg)
{
  perror(msg);
  exit(1);
}

void usage()
{
  fprintf(stderr, "Usage: epoll-listen-fork [unix|inet] [path|port] [et|lt] sleep\n");
  exit(1);
}

int main(int argc, const char * const argv[])
{
  if (argc != 5) {
    usage();
  }

  argv++;
  const char *protocol = *argv++;
  const char *port = *argv++;
  const char *epoll_type = *argv++;
  const int delay = atoi(*argv++);

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
    if (inet_aton("0.0.0.0", &saddr_in.sin_addr) < 0) {
      handle_error("inet_aton");
    }
    saddr = (struct sockaddr *) &saddr_in;
    addrlen = sizeof(saddr_in);
  }
  else {
    usage();
  }

  int epoll_flags;
  if (strcmp(epoll_type, "lt") == 0) {
    epoll_flags = EPOLLIN;
  }
  else if (strcmp(epoll_type, "et") == 0) {
    epoll_flags = EPOLLIN | EPOLLET;
  }
  else {
    usage();
  }

  // Ignore SIGCHLD.
  signal(SIGCHLD, SIG_IGN);
  
  // Create a listening socket.
  int listener = socket(saddr->sa_family, SOCK_STREAM, 0);
  if (listener < 0) {
    handle_error("socket");
  }
  if (bind(listener, (struct sockaddr *) saddr, addrlen) < 0) {
    handle_error("bind");
  }
  if (listen(listener, 1024) < 0) {
    handle_error("listen");
  }

  // Set up epoll with edge triggering on listener.
  struct epoll_event events[16];
  const int epollfd = epoll_create(1024);
  struct epoll_event event = { epoll_flags, 0 };
  event.data.fd = listener;
  epoll_ctl(epollfd, EPOLL_CTL_ADD, listener, &event);
  
  int timeout = 1000;
  int ret, total = 0;
  int start = time(0);
  while ((ret = epoll_wait(epollfd, events, sizeof(events) / sizeof(*events), timeout)) >= 0) {
    int now = time(0);
    int i;
    if (ret > 0) {
      printf("%03d: parent: %d event(s)\n", now-start, ret);
    }
    for (i = 0; i < ret; i++) {
      printf("%03d: parent: forking child %d\n", now-start, ++total);
      switch (fork()) {
      case -1:
	handle_error("fork");
	break;

      case 0:
	// in the child, total is the child number
	now = time(0);
	printf("%03d: child %d (pid %d): started\n", now-start, total, getpid());
	int sock = accept(listener, NULL, 0);
	if (sock < 0) {
	  handle_error("accept");
	}
	now = time(0);
	printf("%03d: child %d: accepted\n", now-start, total);
	sleep(delay);
	now = time(0);
	printf("%03d: child %d: exiting\n", now-start, total);
	exit(0);
      }
    }
  }
  
  return 0;
}
