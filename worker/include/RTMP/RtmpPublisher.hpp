#ifndef MS_RTMP_PUBLISHER_HPP
#define MS_RTMP_PUBLISHER_HPP

#include "RTMP/RtmpSession.hpp"
#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpPublisher : public RTMP::RtmpTransport
	{
	public:
		explicit RtmpPublisher(RtmpRouter* router, RtmpSession* session);
		~RtmpPublisher();

	protected:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;
	};
} // namespace RTMP

#endif