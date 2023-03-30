#define MS_CLASS "RTMP::RtmpTcpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpServer.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpServerSession.hpp"
#include "RTC/PortManager.hpp"

namespace RTMP
{
	RtmpTcpServer::RtmpTcpServer(Listener* listener, std::string& ip)
	  : RTMP::RtmpTcpServer(listener, ip, 1935)
	{
		MS_TRACE();
	}

	RtmpTcpServer::RtmpTcpServer(Listener* listener, std::string& ip, uint16_t port)
	  : // This may throw.
	    ::TcpServerHandler::TcpServerHandler(RTC::PortManager::BindTcp(ip, port)), listener(listener),
	    fixedPort(true)
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("RtmpTcpServer constructor with ip:%s port:%" PRIu16 ".", ip.c_str(), port);
	}

	RtmpTcpServer::~RtmpTcpServer()
	{
		MS_TRACE();
		if (!fixedPort)
		{
			RTC::PortManager::UnbindTcp(this->localIp, this->localPort);
		}
	}

	void RtmpTcpServer::UserOnTcpConnectionAlloc()
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("UserOnTcpConnectionAlloc ");

		// Allocate a new RTMP::RtmpTcpConnection for the RtmpTcpServer to handle it.
		RTMP::RtmpServerSession* session = this->listener->CreateNewServerSession();

		RTMP::RtmpTcpConnection* connection = session->GetConnection();
		// Accept it.
		AcceptTcpConnection(connection);

		// [dming]
		// session 应该注册到RtmpServer里，即this->listener应该保有所有的session，并在connection
		// close的时候，调用其析构函数
		if (this->connections.find(connection) != this->connections.end())
		{
			this->listener->OnRtmpServerSessionCreated(this, session);
		}
		else
		{
			FREEP(session);
		}
	}

	void RtmpTcpServer::UserOnTcpConnectionClosed(::TcpConnectionHandler* connection)
	{
		MS_TRACE();

		MS_DEBUG_DEV_STD(
		  "UserOnTcpConnectionClosed ip=%s, port=%" PRIu16,
		  connection->GetPeerIp().c_str(),
		  connection->GetPeerPort());
		this->listener->OnRtcTcpConnectionClosed(this, static_cast<RTMP::RtmpTcpConnection*>(connection));
	}
} // namespace RTMP