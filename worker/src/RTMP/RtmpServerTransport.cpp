#define MS_CLASS "RTMP::RtmpServerTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpServerTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"

namespace RTMP
{
	RtmpServerTransport::RtmpServerTransport(
	  RTC::Shared* shared,
	  std::string id,
	  RtmpTransport::Listener* listener,
	  bool isPublisher,
	  RtmpServerSession* session)
	  : RtmpTransport(shared, id, listener, isPublisher), session_(session)
	{
		MS_TRACE();
		// NOTE: This may throw.
		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id_,
		  /*channelRequestHandler*/ this,
		  /*payloadChannelRequestHandler*/ this,
		  /*payloadChannelNotificationHandler*/ this);
	}

	RtmpServerTransport::~RtmpServerTransport()
	{
		MS_TRACE();
		this->shared->channelMessageRegistrator->UnregisterHandler(this->id_);
		session_ = nullptr;
	}

	void RtmpServerTransport::FillJson(json& jsonObject) const
	{
		MS_TRACE();

		// Call the parent method.
		RtmpTransport::FillJson(jsonObject);

		jsonObject["isServer"] = true;
	}

	srs_error_t RtmpServerTransport::RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		return RtmpTransport::RecvMessage(tuple, msg);
	}

	RtmpRtmpConnType RtmpServerTransport::GetInfoType()
	{
		return session_->GetInfo()->type;
	}
	int RtmpServerTransport::GetStreamId()
	{
		return session_->GetInfo()->res->stream_id;
	}
	srs_error_t RtmpServerTransport::OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket)
	{
		return session_->decode_message(msg, ppacket);
	}
	srs_error_t RtmpServerTransport::OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id)
	{
		return session_->SendAndFreeMessage(msg, stream_id);
	}
	srs_error_t RtmpServerTransport::OnSendAndFreePacket(RtmpPacket* packet, int stream_id)
	{
		return session_->SendAndFreePacket(packet, stream_id);
	}
} // namespace RTMP