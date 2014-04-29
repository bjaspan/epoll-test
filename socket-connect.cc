/**
 * Connect to a specified socket a specified number of times, read a character
 * from stdin, then exit.
 */

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

void usage()
{
  std::cerr << "Usage: socket-connect [unix|inet] [path|port] count" << std::endl;
  exit(1);
}

int main(int argc, const char * const argv[])
{
  if (argc != 4) {
    usage();
  }

  const char *protocol = argv[1];
  const char *port = argv[2];
  const int count = atoi(argv[3]);

  struct sockaddr_un saddr_un;
  struct sockaddr_in saddr_in;
  struct sockaddr *saddr;
  socklen_t addrlen;

  printf("connecting to %s %s %d times\n", protocol, port, count);

  if (strcmp(protocol, "unix") == 0) {
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

  int i;
  for (i = 0; i < count; i++) {
    printf("%d\n", i+1);
    socket_connect(saddr, addrlen);
  }

  getchar();
  
  return 0;
}
