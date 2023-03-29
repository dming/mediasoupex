#define MS_CLASS "RTMP::RtmpPublisher"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpPublisher.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpRouter.hpp"

namespace RTMP
{

	RtmpPublisher::RtmpPublisher(Listener* listener) : listener_(listener)
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
	srs_error_t RtmpPublisher::RecvMessage(RTC::TransportTuple* /* tuple */, RtmpCommonMessage* msg)
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
			if ((err = listener_->OnDecodeMessage(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			RtmpAutoFree(RtmpPacket, packet);

			// for flash, any packet is republish.
			if (listener_->GetInfo()->type == RtmpRtmpConnFlashPublish)
			{
				// flash unpublish.
				// TODO: maybe need to support republish.
				MS_DEBUG_DEV("flash flash publish finished.");
				return srs_error_new(ERROR_CONTROL_REPUBLISH, "rtmp: republish");
			}

			// for fmle, drop others except the fmle start packet.
			if (dynamic_cast<RtmpFMLEStartPacket*>(packet))
			{
				MS_DEBUG_DEV("Publisher RtmpFMLEStartPacket FMLEUnpublish");
				RtmpFMLEStartPacket* unpublish = dynamic_cast<RtmpFMLEStartPacket*>(packet);
				if ((err = FMLEUnpublish(listener_->GetInfo()->res->stream_id, unpublish->transaction_id)) != srs_success)
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
			listener_->OnPublisherAudio(msg);
		}
		else if (msg->header.is_video())
		{
			listener_->OnPublisherVideo(msg);
		}
		else if (msg->header.is_aggregate())
		{
			listener_->OnPublisherAggregate(msg);
		}
		else if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage is_amf0_data ||  is_amf3_data");
			RTMP::RtmpPacket* packet;
			if ((err = listener_->OnDecodeMessage(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			RtmpAutoFree(RtmpPacket, packet);
			if (dynamic_cast<RtmpOnMetaDataPacket*>(packet))
			{
				RtmpOnMetaDataPacket* metadata = dynamic_cast<RtmpOnMetaDataPacket*>(packet);
				if ((err = listener_->OnPublisherMetaData(msg, metadata)) != srs_success)
				{
					return srs_error_wrap(err, "source consume metadata");
				}
				return err;
			}
		}
		return err;
	}

	srs_error_t RtmpPublisher::FMLEUnpublish(int stream_id, double unpublish_tid)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV_STD("FMLEUnpublish stream_id=%d, unpublish_tid=%f", stream_id, unpublish_tid);
		// publish response onFCUnpublish(NetStream.unpublish.Success)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->command_name = RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH;
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeUnpublishSuccess));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Stop publishing stream."));

			if ((err = listener_->OnSendAndFreePacket(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.unpublish.Success");
			}
			MS_DEBUG_DEV_STD("send NetStream.unpublish.Success");
		}
		// FCUnpublish response
		if (true)
		{
			RtmpFMLEStartResPacket* pkt = new RtmpFMLEStartResPacket(unpublish_tid);
			if ((err = listener_->OnSendAndFreePacket(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send FCUnpublish response");
			}
			MS_DEBUG_DEV_STD("send FCUnpublish response");
		}
		// publish response onStatus(NetStream.Unpublish.Success)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeUnpublishSuccess));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Stream is now unpublished"));
			pkt->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = listener_->OnSendAndFreePacket(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Unpublish.Success");
			}
			MS_DEBUG_DEV_STD("send NetStream.Unpublish.Success");
		}

		return err;
	}
} // namespace RTMP
