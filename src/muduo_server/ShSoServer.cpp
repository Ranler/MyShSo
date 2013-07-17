#include <map>
#include <boost/bind>
#include "muduo/net/TcpServer.h"
#include "ShSoServer.h"


using namespace boost;
using namespace muduo;
using namespace muduo::net;

ShSoServer::ShSoServer(EventLoop *loop, const InetAddress &listenAddr)
  : _loop(loop),
    _server(loop, listenAddr, "ShSoServer")
{
  _server.setConnectionCallback(bind(&ShSoServer::onConnection, this, _1));
  _server.setMessageCallback(bind(&ShSoServer::onMessage, this, _1, _2, _3));
}


void ShSoServer::onServerConnection(const TcpConnectionPtr& conn)
{
  if (conn->connected()) {
    conn->setTcpNoDelay(true);
  } else {
    std::map<string, TunnelPtr>::iterator it = _tunnels.find(conn->name());
    if (it != _tunnels.end()) {
      it->second->stop();
      _tunnels.erase(it);
    }
  }
}



void ShSoServer::onServerMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
  if (_tunnels.find(conn->name()) == _tunnels.end()) {    // tunnel not create
    
  } else {
     const TcpConnectionPtr &clientConn
       = boost::any_cast<const TcpConnectionPtr&>(conn->getContext());
     clientConn->send(buf);
  }
}

