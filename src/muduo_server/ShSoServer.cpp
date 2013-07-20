#include <map>
#include <boost/bind/bind.hpp>
#include "ShSoServer.h"

using namespace boost;
using namespace muduo;
using namespace muduo::net;

ShSoServer::ShSoServer(EventLoop *loop, const InetAddress &listenAddr)
  : _loop(loop),
    _server(loop, listenAddr, "ShSoServer")
{
  _server.setConnectionCallback(bind(&ShSoServer::onServerConnection, this, _1));
  _server.setMessageCallback(bind(&ShSoServer::onServerMessage, this, _1, _2, _3));
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

static shared_ptr<InetAddress> getAddr(Buffer* buf)
{
  int8_t addrtype = buf->peekInt8();
  
  shared_ptr<InetAddress> paddr;
  switch(addrtype) {
  case ShSoServer::ADDRTYPE_IPV4:    /* |addr(4)|+|port(2)|*/
    {
      if (buf->readableBytes() < 7) break;   // wait to next callback

      sockaddr_in addr;
      const void* ip = buf->peek() + 1;
      const void* port = buf->peek() + 5;
      bzero(&addr, sizeof addr);
      addr.sin_family = AF_INET;
      addr.sin_port = *static_cast<const in_port_t*>(port);
      addr.sin_addr = *static_cast<const in_addr*>(ip);
      
      paddr = shared_ptr<InetAddress>(new InetAddress(addr));
    }
      break;
    
  case ShSoServer::ADDRTYPE_DOMAIN:    /* |addr_len(1)|+|host(addr_len)|+|port(2)| */
      break;
    
  case ShSoServer::ADDRTYPE_IPV6:  /* |addr(16)|+|port(2)|*/      
      break;
      
  }
  
  return paddr;
}

void ShSoServer::onServerMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time)
{
  if (_tunnels.find(conn->name()) == _tunnels.end()) {    // tunnel not connected
    shared_ptr<InetAddress> serverAddr = getAddr(buf);
    if (serverAddr) {
      TunnelPtr tunnel(new Tunnel(_loop, *serverAddr, conn));
      tunnel->start();
      _tunnels[conn->name()] = tunnel;
    }
  } else {
     const TcpConnectionPtr &clientConn
       = boost::any_cast<const TcpConnectionPtr&>(conn->getContext());
     clientConn->send(buf);
  }
}

