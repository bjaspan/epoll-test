unix-domain.cc tests epoll edge triggering with a Unix domain listening socket.
The results are not what I expect.

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
