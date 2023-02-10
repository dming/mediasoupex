#define MS_CLASS "RTMP::RtmpTcpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpServer.hpp"
#include "Logger.hpp"
#include "RTC/PortManager.hpp"

namespace RTMP
{
	RtmpTcpServer::RtmpTcpServer(
	  Listener* listener, RTMP::RtmpTcpConnection::Listener* connListener, std::string& ip)
	  : RTMP::RtmpTcpServer(listener, connListener, ip, 1935)
	{
		MS_TRACE();
	}

	RtmpTcpServer::RtmpTcpServer(
	  Listener* listener, RTMP::RtmpTcpConnection::Listener* connListener, std::string& ip, uint16_t port)
	  : // This may throw.
	    ::TcpServerHandler::TcpServerHandler(RTC::PortManager::BindTcp(ip, port)), listener(listener),
	    connListener(connListener)
	{
		MS_TRACE();
		MS_DEBUG_DEV("RtmpTcpServer constructor with ip:%s port:%d", ip.c_str(), port);
	}

	RtmpTcpServer::~RtmpTcpServer()
	{
		MS_TRACE();
	}

	void RtmpTcpServer::UserOnTcpConnectionAlloc()
	{
		MS_TRACE();

		// Allocate a new RTMP::RtmpTcpConnection for the RtmpTcpServer to handle it.
		auto* connection = new RTMP::RtmpTcpConnection(
		  this->connListener, 65536); //[dming] TODO:应该与rtc对应，创建的是RtmpTransport. 叉掉

		// Accept it.
		AcceptTcpConnection(connection);
	}

	void RtmpTcpServer::UserOnTcpConnectionClosed(::TcpConnectionHandler* connection)
	{
		MS_TRACE();

		this->listener->OnRtcTcpConnectionClosed(this, static_cast<RTMP::RtmpTcpConnection*>(connection));
	}
} // namespace RTMP