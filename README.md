This repo contains a number of tools to test listening socket behavior,
particularly when using epoll edge triggering. The tools were written to debug
and gain full understanding of the process-management behavior of PHP-FPM.

# Tools

* epoll-socket-test: Tests epoll edge triggering with a unix or inet
listening socket.  The results are not what I expect.

* epoll-listen-fork: Simulates a socket server using epoll. Listenens on a unix
  or inet socket using either level- or edge-triggering, and forks a child for
  each epoll event which accepts a connection, sleeps for a specified time, and
  then exits.

* socket-connect: Connects to a unix or inet socket a specified number of
  times, then reads a character from stdin and exits.

# epoll-socket-test

## Background

My understanding of epoll edge-triggering is that it returns an event only when
the underlying state changes. If data arrives on a fd, you get an EPOLLIN
event. But if more data arrives on the fd before all the previous data has been
read, you do NOT get another EPOLLIN event. You first have to read the fd until
you get EAGAIN/EWOULDBLOCK; then any newly arrived data will trigger another
EPOLLIN.

This does not seem to be true for listening sockets. It appears that epoll
returns a separate EPOLLIN event for every newly arrived pending connection
when edge triggering is enabled. The epoll(7) man page says that this *can*
happen ("even with edge-triggered epoll, multiple events can be generated upon
receipt of multiple chunks of data"), but does not guarantee that it will.

I have not seen any documentation specifically for epoll edge triggering on
listening sockets. I suspect that it is a consequence of the implementation,
but probably not a guarantee, but am not yet sure.

## Smoking gun

Create a listening Unix domain socket and add it to an epoll instance:

```
socket(PF_FILE, SOCK_STREAM, 0)         = 7
bind(7, {sa_family=AF_FILE, path="/tmp/scratch-path"}, 110) = 0
listen(7, 1024)                         = 0
epoll_create(1024)                      = 8
epoll_ctl(8, EPOLL_CTL_ADD, 7, {EPOLLIN|EPOLLET, {u32=7, u64=7}}) = 0
epoll_wait(8, {}, 16, 0)              = 0
```

In the same process (if you compile the test program with -DINLINE_CONNECT),
connect to the listening socket:

```
socket(PF_FILE, SOCK_STREAM, 0)         = 9
connect(9, {sa_family=AF_FILE, path="/tmp/scratch-path"}, 110) = 0
```

epoll\_wait returns the EPOLLIN event just once, as expected:

```
epoll_wait(8, {{EPOLLIN, {u32=7, u64=7}}}, 16, 0) = 1
epoll_wait(8, {}, 16, 0)              = 0
```

Without calling accept(), open a second connection to the same listening
socket:

```
socket(PF_FILE, SOCK_STREAM, 0)         = 10
connect(10, {sa_family=AF_FILE, path="/tmp/scratch-path"}, 110) = 0
```

The previous pending connection is not yet accepted, so the read state of the
listening socket has not changed. However, epoll\_wait() returns it again:

```
epoll_wait(8, {{EPOLLIN, {u32=7, u64=7}}}, 16, 0) = 1
```

## Test program

```
Usage: epoll-socket-test [unix|inet] [path|port]
```

To build and run:

```
$ c++ -o epoll-socket-test epoll-socket-test.cc
$ ./epoll-socket-test unix /tmp/scratch-path
expected 0 == 1: epoll 1 after connect 2
```

The default build performs the connections via a child process. Add
-DINLINE_CONNECT to connect from the main process. The results are the same.

For more fun, try:

```
strace -ff -o epoll-socket-test.out ./epoll-socket-test unix /tmp/scratch-path
```

and look in epoll-socket-test.out.\<pids\> for the parent and child details.

