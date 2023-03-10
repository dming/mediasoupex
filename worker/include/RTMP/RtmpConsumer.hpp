#ifndef MS_RTMP_CONSUMER_HPP
#define MS_RTMP_CONSUMER_HPP

#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpRouter;
	class RtmpConsumer : public RTMP::RtmpTransport
	{
	public:
		explicit RtmpConsumer(RtmpRouter* router, RtmpSession* session);
		~RtmpConsumer();

	protected:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;
	};
} // namespace RTMP

#endif