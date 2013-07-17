#ifndef _MY_TUNNEL_
#define _MY_TUNNEL_

#include <boost/bind/bind.hpp>
#include "muduo/net/EventLoop.h"
#include "muduo/net/InetAddress.h"
#include "muduo/net/TcpClient.h"
#include "muduo/base/Logging.h"


class Tunnel: public boost::enable_shared_from_this<Tunnel>,
  boost::noncopyable
{
public:
  Tunnel(muduo::net::EventLoop* loop,
	 const muduo::net::InetAddress &serverAddr,
	 const muduo::net::TcpConnectionPtr& severConn);
  ~Tunnel();

  void start();
  void stop();

  // callback functions
  void onClientConn(const muduo::net::TcpConnectionPtr &conn);
  void onClientMsg(const muduo::net::TcpConnectionPtr &conn,
		   muduo::net::Buffer *buf,
		   muduo::Timestamp);  

private:
  muduo::net::TcpClient _client;
  muduo::net::TcpConnectionPtr _serverConn;
};

typedef boost::shared_ptr<Tunnel> TunnelPtr;

#endif
