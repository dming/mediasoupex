#ifndef MS_RTMP_TCP_SERVER_HPP
#define MS_RTMP_TCP_SERVER_HPP

#include "common.hpp"
#include "Rtmp/RtmpSession.hpp"
#include "Rtmp/RtmpTcpConnection.hpp"
#include "handles/TcpConnectionHandler.hpp"
#include "handles/TcpServerHandler.hpp"
#include <string>

namespace RTMP
{
	// LIKE RTC TcpServer
	class RtmpTcpServer : public ::TcpServerHandler
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual RtmpSession* CreateNewSession() = 0;
			virtual void OnRtcTcpConnectionClosed(
			  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection) = 0;
			virtual void OnRtmpSessionCreated(RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpSession* session) = 0;
		};

	public:
		RtmpTcpServer(Listener* listener, std::string& ip);
		RtmpTcpServer(Listener* listener, std::string& ip, uint16_t port);
		~RtmpTcpServer() override;

		/* Pure virtual methods inherited from ::TcpServerHandler. */
	public:
		void UserOnTcpConnectionAlloc() override;
		void UserOnTcpConnectionClosed(::TcpConnectionHandler* connection) override;

	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		bool fixedPort{ false };
	};
} // namespace RTMP

#endif // MS_RTMP_TCP_SERVER_HPP