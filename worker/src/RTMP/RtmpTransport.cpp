#define MS_CLASS "RTMP::RtmpTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"

namespace RTMP
{
	RtmpTransport::RtmpTransport(RTC::Shared* shared, std::string id, Listener* listener, bool isPublisher)
	  : shared(shared), id_(id), listener_(listener), isPublisher_(isPublisher)
	{
		MS_DEBUG_DEV_STD("RtmpTransport constructor..");
		publisher_ = nullptr;
		consumer_  = nullptr;
		if (isPublisher)
		{
			publisher_ = new RtmpPublisher(this);
		}
		else
		{
			consumer_ = new RtmpConsumer(this);
		}
	}

	RtmpTransport::~RtmpTransport()
	{
		MS_DEBUG_DEV_STD("~RtmpTransport ..");
		listener_ = nullptr;
		FREEP(publisher_);
		FREEP(consumer_);
	}

	void RtmpTransport::FillJson(json& jsonObject) const
	{
		MS_TRACE();

		// Add id.
		jsonObject["id"] = this->id_;
	}

	srs_error_t RtmpTransport::RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		if (publisher_)
		{
			return publisher_->RecvMessage(tuple, msg);
		}
		else if (consumer_)
		{
			return consumer_->RecvMessage(tuple, msg);
		}
		return srs_success;
	}

	void RtmpTransport::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();
		switch (request->methodId)
		{
			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->method.c_str());
			}
		}
	}

	void RtmpTransport::HandleRequest(PayloadChannel::PayloadChannelRequest* request)
	{
		MS_TRACE();

		switch (request->methodId)
		{
			default:
			{
				MS_THROW_ERROR("unknown method '%s'", request->method.c_str());
			}
		}
	}

	void RtmpTransport::HandleNotification(PayloadChannel::PayloadChannelNotification* notification)
	{
		MS_TRACE();

		switch (notification->eventId)
		{
			default:
			{
				MS_ERROR("unknown event '%s'", notification->event.c_str());
			}
		}
	}

	RtmpRtmpConnType RtmpTransport::GetInfoType()
	{
		return RtmpRtmpConnUnknown;
	}

	int RtmpTransport::GetStreamId()
	{
		return SRS_DEFAULT_SID;
	}

	srs_error_t RtmpTransport::OnPublisherMetaData(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata)
	{
		return listener_->OnMetaData(msg, metadata);
	}

	srs_error_t RtmpTransport::OnPublisherAudio(RtmpCommonMessage* msg)
	{
		return listener_->OnAudio(msg);
	}

	srs_error_t RtmpTransport::OnPublisherVideo(RtmpCommonMessage* msg)
	{
		return listener_->OnVideo(msg);
	}

	srs_error_t RtmpTransport::OnPublisherAggregate(RtmpCommonMessage* msg)
	{
		return listener_->OnAggregate(msg);
	}
} // namespace RTMP
