#ifndef _MY_SHSOSERVER_
#define _MY_SHSOSERVER_

#include <string>
#include <map>
#include "Tunnel.h"
#include "muduo/net/TcpServer.h"

class ShSoServer
{
 public:
  ShSoServer(muduo::net::EventLoop *loop, 
	     const muduo::net::InetAddress &listenAddr);

  const static int ADDRTYPE_IPV4 = 1;
  const static int ADDRTYPE_DOMAIN = 3;
  const static int ADDRTYPE_IPV6 = 4;
  
 private:
  // callback functions
  void onServerConnection(const muduo::net::TcpConnectionPtr& conn);
  void onServerMessage(const muduo::net::TcpConnectionPtr& conn,
		 muduo::net::Buffer* buf,
		 muduo::Timestamp time);


  muduo::net::EventLoop *_loop;
  muduo::net::TcpServer _server;

  std::map<muduo::string, TunnelPtr> _tunnels;
};

#endif
