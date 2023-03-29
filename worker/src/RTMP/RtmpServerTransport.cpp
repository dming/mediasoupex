#define MS_CLASS "RTMP::RtmpServerTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpServerTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"

namespace RTMP
{
	RtmpServerTransport::RtmpServerTransport(
	  RtmpTransport::Listener* listener, bool isPublisher, RtmpServerSession* session)
	  : RtmpTransport(listener, isPublisher), session_(session)
	{
	}

	RtmpServerTransport::~RtmpServerTransport()
	{
		session_ = nullptr;
	}

	srs_error_t RtmpServerTransport::RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		return RtmpTransport::RecvMessage(tuple, msg);
	}

	RtmpClientInfo* RtmpServerTransport::GetInfo()
	{
		return session_->GetInfo();
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