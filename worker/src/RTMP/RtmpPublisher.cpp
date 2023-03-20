#define MS_CLASS "RTMP::RtmpPublisher"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpPublisher.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpRouter.hpp"

namespace RTMP
{

	RtmpPublisher::RtmpPublisher(RtmpRouter* router, RtmpSession* session)
	  : RTMP::RtmpTransport::RtmpTransport(router, session)
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("RtmpPublisher constructor..");
	}

	RtmpPublisher::~RtmpPublisher()
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("~RtmpPublisher");
	}

	// as srs  RtmpRtmpConn::handle_publish_message
	srs_error_t RtmpPublisher::OnRecvMessage(RTC::TransportTuple* /* tuple */, RtmpCommonMessage* msg)
	{
		srs_error_t err          = srs_success;
		static size_t VAMsgCount = 0;

		if (msg->header.message_type == RTMP_MSG_VideoMessage || msg->header.message_type == RTMP_MSG_AudioMessage)
		{
			if (VAMsgCount % 300 == 0)
			{
				MS_DEBUG_DEV_STD("RtmpPublisher msg type is %d", msg->header.message_type);
			}
			VAMsgCount++;
		}
		else
		{
			MS_DEBUG_DEV_STD("RtmpPublisher msg type is %d", msg->header.message_type);
		}

		if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage is_amf0_command ||  is_amf3_command ");
			RTMP::RtmpPacket* packet;
			if ((err = session_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			RtmpAutoFree(RtmpPacket, packet);

			// for flash, any packet is republish.
			if (session_->GetInfo()->type == RtmpRtmpConnFlashPublish)
			{
				// flash unpublish.
				// TODO: maybe need to support republish.
				MS_DEBUG_DEV("flash flash publish finished.");
				return srs_error_new(ERROR_CONTROL_REPUBLISH, "rtmp: republish");
			}

			// for fmle, drop others except the fmle start packet.
			if (dynamic_cast<RtmpFMLEStartPacket*>(packet))
			{
				MS_DEBUG_DEV("Publisher RtmpFMLEStartPacket fmle_unpublish");
				RtmpFMLEStartPacket* unpublish = dynamic_cast<RtmpFMLEStartPacket*>(packet);
				if ((err = session_->fmle_unpublish(session_->GetInfo()->res->stream_id, unpublish->transaction_id)) != srs_success)
				{
					return srs_error_wrap(err, "rtmp: republish");
				}
				// [dming] TODO: 补充router关闭publisher的逻辑
				return srs_error_new(ERROR_CONTROL_REPUBLISH, "rtmp: republish");
			}

			MS_DEBUG_DEV("fmle ignore AMF0/AMF3 command message.");
			return err;
		}
		else if (msg->header.is_audio())
		{
			router_->OnAudio(msg);
		}
		else if (msg->header.is_video())
		{
			router_->OnVideo(msg);
		}
		else if (msg->header.is_aggregate())
		{
			router_->OnAggregate(msg);
		}
		else if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage is_amf0_data ||  is_amf3_data");
			RTMP::RtmpPacket* packet;
			if ((err = session_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			RtmpAutoFree(RtmpPacket, packet);
			if (dynamic_cast<RtmpOnMetaDataPacket*>(packet))
			{
				RtmpOnMetaDataPacket* metadata = dynamic_cast<RtmpOnMetaDataPacket*>(packet);
				if ((err = router_->on_meta_data(msg, metadata)) != srs_success)
				{
					return srs_error_wrap(err, "source consume metadata");
				}
				return err;
			}
		}
		return err;
	}
} // namespace RTMP