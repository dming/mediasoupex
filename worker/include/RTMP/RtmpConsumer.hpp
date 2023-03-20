#ifndef MS_RTMP_CONSUMER_HPP
#define MS_RTMP_CONSUMER_HPP

#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpConsumer : public RTMP::RtmpTransport
	{
	public:
		explicit RtmpConsumer(RtmpRouter* router, RtmpSession* session);
		~RtmpConsumer();

	protected:
		srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;

	public:
		virtual srs_error_t enqueue(RtmpSharedPtrMessage* shared_msg, bool atc, RtmpRtmpJitterAlgorithm ag);

		// Get current client time, the last packet time.
		virtual int64_t get_time();

	private:
		// when client send the pause message.
		virtual srs_error_t on_play_client_pause(bool is_pause);

	private:
		RtmpRtmpJitter* jitter_;
		bool b_paused_;
	};
} // namespace RTMP

#endif