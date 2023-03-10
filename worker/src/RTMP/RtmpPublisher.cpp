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

	// as srs  SrsRtmpConn::handle_publish_message
	srs_error_t RtmpPublisher::OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
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

		RTMP::RtmpPacket* packet;
		if (msg->header.is_user_control_message())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpUserControlPacket ");
		}
		else if (msg->header.is_window_ackledgement_size())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpSetWindowAckSizePacket ");
		}
		else if (msg->header.is_set_chunk_size())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpSetChunkSizePacket ");
		}
		else if (msg->header.is_ackledgement())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpAcknowledgementPacket ");
		}
		else if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			if ((err = session_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpConnectAppPacket* m_packet = dynamic_cast<RtmpConnectAppPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpConnectAppPacket ");
				return err;
			}
			else if (RtmpCreateStreamPacket* m_packet = dynamic_cast<RtmpCreateStreamPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpCreateStreamPacket ");
				return err;
			}
			else if (RtmpPlayPacket* m_packet = dynamic_cast<RtmpPlayPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpPlayPacket ");
				return err;
			}
			else if (RtmpPausePacket* m_packet = dynamic_cast<RtmpPausePacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpPausePacket ");
			}
			else if (RtmpFMLEStartPacket* m_packet = dynamic_cast<RtmpFMLEStartPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpFMLEStartPacket ");
				return err;
			}
			else if (RtmpPublishPacket* m_packet = dynamic_cast<RtmpPublishPacket*>(packet))
			{
				MS_DEBUG_DEV_STD(
				  "====>Publisher OnRecvMessage RtmpPublishPacket command name is %s ",
				  m_packet->command_name.c_str());
				return err;
			}
			else if (RtmpCloseStreamPacket* m_packet = dynamic_cast<RtmpCloseStreamPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpCloseStreamPacket ");
			}
			else if (RtmpCallPacket* m_packet = dynamic_cast<RtmpCallPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage RtmpCallPacket ");
			}
		}
		else if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			MS_DEBUG_DEV_STD("====>Publisher OnRecvMessage is_amf0_data ||  is_amf3_data");
			if ((err = session_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
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
		else if (msg->header.is_audio())
		{
			router_->OnAudio(msg);
		}
		else if (msg->header.is_video())
		{
			router_->OnVideo(msg);
		}
		return err;
	}
} // namespace RTMP