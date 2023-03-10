#define MS_CLASS "RTMP::RtmpConsumer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpConsumer.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpRouter.hpp"

namespace RTMP
{

	RtmpConsumer::RtmpConsumer(RtmpRouter* router, RtmpSession* session)
	  : RTMP::RtmpTransport::RtmpTransport(router, session)
	{
	}

	RtmpConsumer::~RtmpConsumer()
	{
		MS_DEBUG_DEV_STD("~RtmpConsumer");
	}

	srs_error_t RtmpConsumer::OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		srs_error_t err = srs_success;
		return err;
	}
} // namespace RTMP