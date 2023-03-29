#ifndef MS_RTMP_CONSUMER_HPP
#define MS_RTMP_CONSUMER_HPP

#include "RTMP/Interface/IRtmpPublisherConsumer.hpp"
#include "RTMP/RtmpJitter.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"

namespace RTMP
{
	class RtmpConsumer : public IRtmpPublisherConsumer
	{
	public:
		class Listener : virtual public IRtmpPublisherConsumer::Listener
		{
		public:
			virtual ~Listener() = default;
		};

	public:
		explicit RtmpConsumer(RtmpConsumer::Listener* listener);
		virtual ~RtmpConsumer();

	public:
		virtual srs_error_t enqueue(RtmpSharedPtrMessage* shared_msg, bool atc, RtmpRtmpJitterAlgorithm ag);

		/* Pure virtual methods inherited from RTMP::IRtmpPublisherConsumer. */
	public:
		srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;

		// Get current client time, the last packet time.
		virtual int64_t get_time();

	protected:
		// When client(type is play) send pause message,
		// if is_pause, response the following packets:
		//     onStatus(NetStream.Pause.Notify)
		//     StreamEOF
		// if not is_pause, response the following packets:
		//     onStatus(NetStream.Unpause.Notify)
		//     StreamBegin
		virtual srs_error_t on_play_client_pause(bool is_pause);

	protected:
		RtmpConsumer::Listener* listener_;
		RtmpRtmpJitter* jitter_;
		bool b_paused_;
	};
} // namespace RTMP

#endif