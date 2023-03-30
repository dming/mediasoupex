#ifndef MS_RTMP_DIRECT_TRANSPORT_HPP
#define MS_RTMP_DIRECT_TRANSPORT_HPP

#include "RTMP/RtmpServerSession.hpp"
#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpDirectTransport : public RtmpTransport
	{
	public:
		explicit RtmpDirectTransport(
		  RTC::Shared* shared, std::string id, RtmpTransport::Listener* listener, bool IsPublisher);

		virtual ~RtmpDirectTransport();

	public:
		void FillJson(json& jsonObject) const override;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from PayloadChannel::PayloadChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(PayloadChannel::PayloadChannelNotification* notification) override;

		/* Pure virtual methods inherited from IRtmpTransport. */
	public:
		srs_error_t OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket) override;
		srs_error_t OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id) override;
		srs_error_t OnSendAndFreePacket(RtmpPacket* packet, int stream_id) override;

	private:
		RtmpParser* parser_;
	};
} // namespace RTMP
#endif