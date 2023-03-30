#define MS_CLASS "RTMP::RtmpDirectTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpDirectTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "RTMP/RtmpParser.hpp"

namespace RTMP
{
	RtmpDirectTransport::RtmpDirectTransport(
	  RTC::Shared* shared, std::string id, RtmpTransport::Listener* listener, bool isPublisher)
	  : RtmpTransport(shared, id, listener, isPublisher), parser_(new RtmpParser())
	{
		MS_TRACE();
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id_,
		  /*channelRequestHandler*/ this,
		  /*payloadChannelRequestHandler*/ this,
		  /*payloadChannelNotificationHandler*/ this);
	}

	RtmpDirectTransport::~RtmpDirectTransport()
	{
		MS_TRACE();
		this->shared->channelMessageRegistrator->UnregisterHandler(this->id_);
		FREEP(parser_);
	}

	void RtmpDirectTransport::FillJson(json& jsonObject) const
	{
		MS_TRACE();

		// Call the parent method.
		RtmpTransport::FillJson(jsonObject);
	}

	srs_error_t RtmpDirectTransport::OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket)
	{
		return parser_->decode_message(msg, ppacket);
	}

	srs_error_t RtmpDirectTransport::OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id)
	{
		json data            = json::object();
		data["message_type"] = msg->messageType();
		data["timestamp"]    = msg->timestamp;
		data["size"]         = msg->size;

		// Notify the Node DirectTransport.
		this->shared->payloadChannelNotifier->Emit(
		  this->id_, "rtmpmsg", data, (const uint8_t*)msg->payload, msg->size);

		FREEP(msg);

		return srs_success;
	}

	srs_error_t RtmpDirectTransport::OnSendAndFreePacket(RtmpPacket* packet, int stream_id)
	{
		// just delete packet
		FREEP(packet);
		return srs_success;
	}

	void RtmpDirectTransport::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();
		switch (request->methodId)
		{
			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->method.c_str());
			}
		}
	}

	void RtmpDirectTransport::HandleNotification(PayloadChannel::PayloadChannelNotification* notification)
	{
		MS_TRACE();

		switch (notification->eventId)
		{
			default:
			{
				MS_ERROR("unknown event '%s'", notification->event.c_str());
			}
		}
	}
} // namespace RTMP