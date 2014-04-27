unix-domain.cc tests epoll edge triggering with a Unix domain listening socket.
The results are not what I expect.

# Background

My understanding of epoll edge-triggering is that it returns an event only when
the underlying state changes. If data arrives on a fd, you get an EPOLLIN
event. But if more data arrives on the fd before all the previous data has been
read, you do NOT get another EPOLLIN event. You first have to read the socket
until you get EGAIN/EWOULDBLOCK; then any newly arrived data will trigger
another EPOLLIN.

For a listening socket, a pending connection triggers EPOLLIN. I would expect a
second connection arriving before the first one is accept()ed not to trigger
another EPOLLIN with edge triggering, but apparently it does.

# Test program

```
$ c++ -o unix-domain unix-domain.cc
$ ./unix-domain /tmp/scratch-path
expected 0 == 1: epoll 1 after connect 2
```

For more fun, try:

```
strace -ff -o unix-domain.out ./unix-domain /tmp/scratch-path
```

and look in unix-domain.out.\<pids\> for the parent and child details.
