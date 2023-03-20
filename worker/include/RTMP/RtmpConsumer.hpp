#ifndef MS_RTMP_CONSUMER_HPP
#define MS_RTMP_CONSUMER_HPP

#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpRouter;
	class RtmpRtmpJitter;
	class RtmpConsumer : public RTMP::RtmpTransport
	{
	public:
		explicit RtmpConsumer(RtmpRouter* router, RtmpSession* session);
		~RtmpConsumer();

	protected:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;

	public:
		// Get current client time, the last packet time.
		virtual int64_t get_time();

	private:
		RtmpRtmpJitter* jitter_;
	};
} // namespace RTMP

#endif