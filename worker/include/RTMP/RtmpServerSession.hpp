#ifndef MS_RTMP_SERVER_SESSION_HPP
#define MS_RTMP_SERVER_SESSION_HPP

#include "RTMP/RtmpInfo.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTC/TransportTuple.hpp"
// #include <mutex>

namespace RTMP
{
	class RtmpRouter;
	/**
	 * RtmpServerSession
	 * 对应的是RtcTransport，作用包括：
	 * 1. connection, 处理链接
	 * 2. 处理和保持链路状态，包括所有的invoke命令
	 * 3. 如果是publisher，则向RtmpServer创建RtmpRouter. 一个router等于一个直播间 --后面补充
	 * 4. 持有RtmpPublisher或者RtmpPlayer对象，两者互斥，拥有的对象表明Transport的身份。--后面补充
	 */
	class RtmpServerSession : public RTMP::RtmpTcpConnection::Listener
	{
	public:
		class ServerListener
		{
		public:
			// virtual srs_error_t OnServerSessionHandshaked() = 0; // 为了将自身从rtmpServer 的un handshake
			//                                                      // session list里删除
			virtual srs_error_t FetchOrCreateRouter(RtmpRequest* req, RtmpRouter** pps) = 0;
		};

		class Listener
		{
		public:
			virtual srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) = 0;
		};

	public:
		RtmpServerSession(ServerListener* serverListener);
		~RtmpServerSession();

		/* Pure virtual methods inherited from RTMP::RtmpTcpConnection::Listener. */
	public:
		void OnTcpConnectionPacketReceived(
		  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg) override;

	public:
		RtmpTcpConnection* GetConnection()
		{
			return connection_;
		}
		RtmpClientInfo* GetInfo()
		{
			return info_;
		}
		bool IsPublisher()
		{
			return info_->IsPublisher();
		}
		std::string GetStreamUrl()
		{
			return info_->req->get_stream_url();
		}

		int GetStreamId()
		{
			return info_->res->stream_id;
		}
		// Decode bytes oriented RTMP message to RTMP packet,
		// @param ppacket, output decoded packet,
		//       always nullptr if error, never nullptr if success.
		// @return error when unknown packet, error when decode failed.
		srs_error_t decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket)
		{
			return connection_->decode_message(msg, ppacket);
		}

		srs_error_t SendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id)
		{
			return connection_->send_and_free_message(msg, stream_id);
		}
		srs_error_t SendAndFreePacket(RtmpPacket* packet, int stream_id)
		{
			return connection_->send_and_free_packet(packet, stream_id);
		}

	private:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg);
		// handle Packet Functions
		srs_error_t HandleRtmpConnectAppPacket(RtmpConnectAppPacket* packet);
		srs_error_t HandleRtmpFMLEStartPacket(RtmpFMLEStartPacket* packet);
		srs_error_t HandleRtmpCreateStreamPacket(RtmpCreateStreamPacket* packet);
		srs_error_t HandleRtmpPublishPacket(RtmpPublishPacket* packet);
		srs_error_t HandleRtmpPlayPacket(RtmpPlayPacket* packet);
		// @param server_ip the ip of server.
		srs_error_t response_connect_app(RtmpRequest* req, const char* server_ip);

	private:
		// About the rtmp client.
		RtmpClientInfo* info_;

		RtmpTcpConnection* connection_;
		ServerListener* serverListener_;
		Listener* listener_;

		// std::mutex messageMutex;
	};
} // namespace RTMP
#endif