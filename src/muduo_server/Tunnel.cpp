#include "Tunnel.h"

using namespace muduo;
using namespace muduo::net;

Tunnel::Tunnel(EventLoop* loop,
	       const InetAddress &serverAddr,
	       const TcpConnectionPtr& serverConn)
  : _serverConn(serverConn),
    _client(loop, serverAddr, serverConn->name())
{
  LOG_INFO << serverConn->name() << " Tunnel cctor";
  _client.setConnectionCallback(
    boost::bind(&Tunnel::onClientConn, shared_from_this(), _1));
  _client.setMessageCallback(
    boost::bind(&Tunnel::onClientMsg, shared_from_this(), _1, _2, _3));
}
 
Tunnel::~Tunnel()
{
  LOG_INFO << _serverConn->name() << " Tunnel dctor";
}

void Tunnel::start()
{
  _client.connect();
}

void Tunnel::stop()
{
  _client.disconnect();
}

void Tunnel::onClientConn(const TcpConnectionPtr &conn)
{
  if (conn->connected()) {
    conn->setTcpNoDelay(true);    // check
    _serverConn->setContext(conn);
  } else {
    _client.setConnectionCallback(defaultConnectionCallback);
    _client.setMessageCallback(defaultMessageCallback);
    if (_serverConn) {
      _serverConn->setContext(boost::any());
      _serverConn->shutdown();
    }
  }
}

void Tunnel::onClientMsg(const TcpConnectionPtr &conn,
		 Buffer *buf,
		 Timestamp)
{
  if (_serverConn) {
    _serverConn->send(buf);
  } else {
    buf->retrieveAll();
    abort();
  }
}
