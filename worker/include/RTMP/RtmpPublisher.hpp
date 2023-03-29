#ifndef MS_RTMP_PUBLISHER_HPP
#define MS_RTMP_PUBLISHER_HPP

#include "RTMP/Interface/IRtmpPublisherConsumer.hpp"
#include "RTMP/RtmpServerSession.hpp"

namespace RTMP
{
	class RtmpPublisher : public IRtmpPublisherConsumer
	{
	public:
		class Listener : virtual public IRtmpPublisherConsumer::Listener
		{
		public:
			virtual ~Listener() = default;
			virtual srs_error_t OnPublisherMetaData(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata) = 0;
			virtual srs_error_t OnPublisherAudio(RtmpCommonMessage* msg)     = 0;
			virtual srs_error_t OnPublisherVideo(RtmpCommonMessage* msg)     = 0;
			virtual srs_error_t OnPublisherAggregate(RtmpCommonMessage* msg) = 0;
		};

	public:
		explicit RtmpPublisher(Listener* listener);
		virtual ~RtmpPublisher();

		/* Pure virtual methods inherited from RTMP::IRtmpPublisherConsumer. */
	public:
		srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;

	protected:
		// process the FMLE unpublish event.
		// @unpublish_tid the unpublish request transaction id.
		virtual srs_error_t FMLEUnpublish(int stream_id, double unpublish_tid);

	protected:
		Listener* listener_;
	};
} // namespace RTMP

#endif