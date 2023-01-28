
#ifndef MS_RTMP_SERVER_HPP
#define MS_RTMP_SERVER_HPP

#include "RTC/Shared.hpp"
#include "RTMP/RtmpTcpServer.hpp"
#include "RTMP/RtmpTransportManager.hpp"
#include <string>

namespace RTMP
{
    /**
     * Rtmp Server 用于listen Rtmp tcp connection，并创建对应的RtmpTransport
     * 由worker在nodejs请求后创建出RtmpServer. config由nodejs提供。
     * 
    */
	class RtmpServer : public RTMP::RtmpTcpServer::Listener,
                       public RTMP::RtmpTcpConnection::Listener,
                       public Channel::ChannelSocket::RequestHandler 
    {
    public:
        RtmpServer(RTC::Shared* shared, const std::string& id, json& data);
        ~RtmpServer();

        /* Methods inherited from Channel::ChannelSocket::RequestHandler. */
    public:
        void HandleRequest(Channel::ChannelRequest* request) override;

        /* Pure virtual methods inherited from RTMP::RtmpTcpServer::Listener. */
    public:
        void OnRtcTcpConnectionClosed(RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection) override;

        /* Pure virtual methods inherited from RTMP::RtmpTcpConnection::Listener. */
    public:
        void OnTcpConnectionPacketReceived(
        RTMP::RtmpTcpConnection* connection, const uint8_t* data, size_t len) override;
    
    public:
    // Passed by argument.
    const std::string id;

    private:
        // Passed by argument.
        RTC::Shared* shared{ nullptr };
        RTMP::RtmpTcpServer* tcpServer;
        //管理rtmp 链接，一个链接对应一个streamUrl以及链路. RtmpTransportManager
        RTMP::RtmpTransportManager transportManager;
	};
} // namespace RTMP
#endif