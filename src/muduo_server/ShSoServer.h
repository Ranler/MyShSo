#ifndef _MY_SHSOSERVER_
#define _MY_SHSOSERVER_

#include "Tunnel.h"
#include "muduo/net/TcpServer.h"

class ShSoServer
{
 public:
  ShSoServer(muduo::net::EventLoop *loop, 
	     const muduo::net::InetAddress &listenAddr);
 private:
  // callback functions
  void onServerConnection(const muduo::net::TcpConnectionPtr& conn);
  void onServerMessage(const muduo::net::TcpConnectionPtr& conn,
		 muduo::net::Buffer* buf,
		 muduo::Timestamp time);


  muduo::net::EventLoop *_loop;
  muduo::net::TcpServer _server;

  std::map<string, TunnelPtr> _tunnels;
};

#endif
