#ifndef MS_RTMP_TRANSPORT_HPP
#define MS_RTMP_TRANSPORT_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpConsumer.hpp"
#include "RTMP/RtmpInfo.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPublisher.hpp"
#include "RTC/Shared.hpp"
#include "RTC/TransportTuple.hpp"
#include <nlohmann/json.hpp>

namespace RTMP
{
	class RtmpTransport : public RtmpConsumer::Listener,
	                      public RtmpPublisher::Listener,
	                      public Channel::ChannelSocket::RequestHandler,
	                      public PayloadChannel::PayloadChannelSocket::RequestHandler,
	                      public PayloadChannel::PayloadChannelSocket::NotificationHandler
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
		explicit RtmpTransport(
		  RTC::Shared* shared, std::string id, Listener* router, bool IsPublisher); // router 应该为Listener
		virtual ~RtmpTransport();

	public:
		virtual void FillJson(json& jsonObject) const;

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

		/* Methods inherited from PayloadChannel::PayloadChannelSocket::RequestHandler. */
	public:
		void HandleRequest(PayloadChannel::PayloadChannelRequest* request) override;

		/* Methods inherited from PayloadChannel::PayloadChannelSocket::NotificationHandler. */
	public:
		void HandleNotification(PayloadChannel::PayloadChannelNotification* notification) override;

		/* Methods inherited from IRtmpPublisherConsumer::Listener. */
	public:
		virtual RtmpRtmpConnType GetInfoType() override;
		virtual int GetStreamId() override;
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

		std::string GetId()
		{
			return id_;
		}

	protected:
		Listener* listener_;

		RtmpPublisher* publisher_;
		RtmpConsumer* consumer_;

		bool isPublisher_;

		RTC::Shared* shared;
		std::string id_;
	};

} // namespace RTMP

#endif