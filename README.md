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

# Smoking gun

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

