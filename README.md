MyShSo
======

My ShadowSocks. 

Reference on [shadowsocks(Python/gevent)](https://github.com/clowwindy/shadowsocks) and [shadowsocks-libuv(C/libuv)](https://github.com/dndx/shadowsocks-libuv).

### raw socks

This one use raw sockets api of UNIX/Linux OS.
It create a socket through `socket()`, `bind()` it to a given port on server, `listen()` the port and `accept()` new connection from client.
And then `fork()` a new process to handle the connection.
So this is a multiprocess implementation of shadowsocks.

The handle function create a encrypted tunnel between client and remote server.
To improve network I/O efficiency and keep portable, use `poll()` for I/O multiplexing.
