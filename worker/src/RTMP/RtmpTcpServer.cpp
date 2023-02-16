#define MS_CLASS "RTMP::RtmpTcpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpServer.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpTransport.hpp"
#include "RTC/PortManager.hpp"

namespace RTMP
{
	RtmpTcpServer::RtmpTcpServer(Listener* listener, std::string& ip)
	  : RTMP::RtmpTcpServer(listener, ip, 1935)
	{
		MS_TRACE();
	}

	RtmpTcpServer::RtmpTcpServer(Listener* Listener, std::string& ip, uint16_t port)
	  : // This may throw.
	    ::TcpServerHandler::TcpServerHandler(RTC::PortManager::BindTcp(ip, port)), listener(listener)
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
		auto transport = new RTMP::RtmpTransport(); // [dming] transport 应该回调给listener

		// Accept it.
		AcceptTcpConnection(transport->GetConnection());
	}

	void RtmpTcpServer::UserOnTcpConnectionClosed(::TcpConnectionHandler* connection)
	{
		MS_TRACE();

		this->listener->OnRtcTcpConnectionClosed(this, static_cast<RTMP::RtmpTcpConnection*>(connection));
	}
} // namespace RTMP