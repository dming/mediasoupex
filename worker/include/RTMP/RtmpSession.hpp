

/**
 * Rtmp Transport , like SRS SrsLiveSource. Managed by RtmpSessionManager.
 * provide producer and consumers.
 */

#ifndef MS_RTMP_SESSION_HPP
#define MS_RTMP_SESSION_HPP

#include "RTMP/RtmpInfo.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTC/TransportTuple.hpp"
// #include <mutex>

namespace RTMP
{
	class RtmpServer;
	class RtmpRouter;
	class RtmpTransport;
	/**
	 * RtmpSession
	 * 对应的是RtcTransport，作用包括：
	 * 1. connection, 处理链接
	 * 2. 处理和保持链路状态，包括所有的invoke命令
	 * 3. 如果是publisher，则向RtmpServer创建RtmpRouter. 一个router等于一个直播间 --后面补充
	 * 4. 持有RtmpPublisher或者RtmpPlayer对象，两者互斥，拥有的对象表明Transport的身份。--后面补充
	 */
	class RtmpSession : public RTMP::RtmpTcpConnection::Listener
	{
	public:
		RtmpSession(RtmpServer* rtmpServer);
		~RtmpSession();

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

	public:
		// process the FMLE unpublish event.
		// @unpublish_tid the unpublish request transaction id.
		virtual srs_error_t fmle_unpublish(int stream_id, double unpublish_tid);
		// When client(type is play) send pause message,
		// if is_pause, response the following packets:
		//     onStatus(NetStream.Pause.Notify)
		//     StreamEOF
		// if not is_pause, response the following packets:
		//     onStatus(NetStream.Unpause.Notify)
		//     StreamBegin
		virtual srs_error_t on_play_client_pause(int stream_id, bool is_pause);

	private:
		// About the rtmp client.
		RtmpClientInfo* info_;

		RtmpTcpConnection* connection_;
		RtmpServer* rtmpServer_;
		RtmpTransport* transport_;

		// std::mutex messageMutex;
	};
} // namespace RTMP
#endif