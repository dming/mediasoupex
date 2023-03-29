#ifndef MS_RTMP_TRANSPORT_HPP
#define MS_RTMP_TRANSPORT_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpConsumer.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPublisher.hpp"
#include "RTC/TransportTuple.hpp"

namespace RTMP
{
	class RtmpTransport : public RtmpConsumer::Listener, public RtmpPublisher::Listener
	{
	public:
		class Listener // 其实就是router
		{
		public:
			virtual ~Listener() = default;
			// judge rtmp router whether has publisher
			virtual bool IsPublishing() = 0;

			virtual srs_error_t OnMetaData(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata) = 0;
			virtual srs_error_t OnAudio(RtmpCommonMessage* shared_audio)                           = 0;
			virtual srs_error_t OnVideo(RtmpCommonMessage* shared_video)                           = 0;
			virtual srs_error_t OnAggregate(RtmpCommonMessage* shared_video)                       = 0;
		};

	public:
		explicit RtmpTransport(Listener* router, bool IsPublisher); // router 应该为Listener
		virtual ~RtmpTransport();

		/* Methods inherited from IRtmpPublisherConsumer::Listener. */
	public:
		virtual RtmpClientInfo* GetInfo()                                                  = 0;
		virtual srs_error_t OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket)  = 0;
		virtual srs_error_t OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id) = 0;
		virtual srs_error_t OnSendAndFreePacket(RtmpPacket* packet, int stream_id)         = 0;

		/* Methods inherited from RtmpPublisher::Listener. */
	public:
		virtual srs_error_t OnPublisherMetaData(
		  RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata) override;
		virtual srs_error_t OnPublisherAudio(RtmpCommonMessage* msg) override;
		virtual srs_error_t OnPublisherVideo(RtmpCommonMessage* msg) override;
		virtual srs_error_t OnPublisherAggregate(RtmpCommonMessage* msg) override;

	public:
		srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg);

		inline bool IsPublisher()
		{
			return publisher_ != nullptr;
		}
		inline bool IsConsumer()
		{
			return consumer_ != nullptr;
		}

		RtmpPublisher* GetPublisher()
		{
			return publisher_;
		}
		RtmpConsumer* GetConsumer()
		{
			return consumer_;
		}

	protected:
		Listener* listener_;

		RtmpPublisher* publisher_;
		RtmpConsumer* consumer_;

		bool isPublisher_;
	};

} // namespace RTMP

#endif