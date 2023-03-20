#define MS_CLASS "RTMP::RtmpTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpRouter.hpp"

namespace RTMP
{
	RtmpTransport::RtmpTransport(RtmpRouter* router, RtmpSession* session)
	  : router_(router), session_(session)
	{
		MS_DEBUG_DEV_STD("RtmpTransport constructor..");
	}

	RtmpTransport::~RtmpTransport()
	{
		MS_DEBUG_DEV_STD("~RtmpTransport ..");
		router_  = nullptr;
		session_ = nullptr;
	}

	srs_error_t RtmpTransport::RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		// MS_DEBUG_DEV_STD("RtmpTransport RecvMessage..");
		return OnRecvMessage(tuple, msg);
	}

	srs_error_t RtmpTransport::send_and_free_message(RtmpSharedPtrMessage* msg, int stream_id)
	{
		return session_->GetConnection()->send_and_free_message(msg, stream_id);
	}
} // namespace RTMP
