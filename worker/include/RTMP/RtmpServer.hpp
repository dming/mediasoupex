
#ifndef MS_RTMP_SERVER_HPP
#define MS_RTMP_SERVER_HPP

#include "RTMP/RtmpServerSession.hpp"
#include "RTMP/RtmpTcpServer.hpp"
#include "RtmpRouter.hpp"
#include "RTC/Shared.hpp"
#include <map>
#include <stdint.h>
#include <string>

namespace RTMP
{
	/**
	 * Rtmp Server 用于listen Rtmp tcp connection，并创建对应的RtmpSession
	 * 由worker在nodejs请求后创建出RtmpServer. config由nodejs提供。
	 *
	 */
	class RtmpServer : public RTMP::RtmpTcpServer::Listener,
	                   public Channel::ChannelSocket::RequestHandler,
	                   public RtmpServerSession::ServerListener
	{
	public:
		RtmpServer(RTC::Shared* shared, const std::string& id, json& data);
		~RtmpServer();

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Pure virtual methods inherited from RTMP::RtmpTcpServer::Listener. */
	public:
		RtmpServerSession* CreateNewServerSession() override;
		void OnRtcTcpConnectionClosed(
		  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection) override;
		void OnRtmpServerSessionCreated(
		  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpServerSession* session) override;

	public:
		RtmpRouter* FetchRouter(RtmpRequest* req);
		RtmpRouter* FetchRouter(std::string streamUrl);

		/* Pure virtual methods inherited from RtmpServerSession::ServerListener. */
		srs_error_t FetchOrCreateRouter(RtmpRequest* req, RtmpRouter** pps) override;

	public:
		// Passed by argument.
		const std::string id;

	private:
		// Passed by argument.
		RTC::Shared* shared{ nullptr };
		RTMP::RtmpTcpServer* tcpServer;
		std::map<uint64_t, RTMP::RtmpServerSession*> sessions_;
		std::map<std::string, RTMP::RtmpRouter*> routers_;
	};
} // namespace RTMP
#endif