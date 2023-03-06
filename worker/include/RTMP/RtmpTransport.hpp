

/**
 * Rtmp Transport , like SRS SrsLiveSource. Managed by RtmpTransportManager.
 * provide producer and consumers.
 */

#ifndef MS_RTMP_TRANSPORT_HPP
#define MS_RTMP_TRANSPORT_HPP

#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpProtocol.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTC/TransportTuple.hpp"
namespace RTMP
{
	/**
	 * RtmpTransport
	 * 对应的是RtcTransport，作用包括：
	 * 1. connection, 处理链接
	 * 2. 处理和保持链路状态，包括所有的invoke命令
	 * 3. 如果是publisher，则向RtmpServer创建RtmpRouter. 一个router等于一个直播间 --后面补充
	 * 4. 持有RtmpPublisher或者RtmpPlayer对象，两者互斥，拥有的对象表明Transport的身份。--后面补充
	 */
	class RtmpTransport : public RTMP::RtmpTcpConnection::Listener
	{
	public:
		RtmpTransport();
		~RtmpTransport();

		/* Pure virtual methods inherited from RTMP::RtmpTcpConnection::Listener. */
	public:
		void OnTcpConnectionPacketReceived(
		  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg) override;

	public:
		RtmpTcpConnection* GetConnection()
		{
			return connection;
		}

	private:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg);
		// handle Packet Functions
		srs_error_t HandleRtmpConnectAppPacket(RtmpConnectAppPacket* packet);
		srs_error_t HandleRtmpFMLEStartPacket(RtmpFMLEStartPacket* packet);
		srs_error_t HandleRtmpCreateStreamPacket(RtmpCreateStreamPacket* packet);
		srs_error_t HandleRtmpPublishPacket(RtmpPublishPacket* packet);
		// @param server_ip the ip of server.
		srs_error_t response_connect_app(RtmpRequest* req, const char* server_ip);

	private:
		RtmpTcpConnection* connection;
		RtmpProtocol* protocol;
		// TODO: 增加SrsClientInfo保存客户端信息
		// About the rtmp client.
		RtmpClientInfo* info;
	};
} // namespace RTMP
#endif