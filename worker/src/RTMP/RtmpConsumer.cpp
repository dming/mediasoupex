#define MS_CLASS "RTMP::RtmpConsumer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpConsumer.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"

namespace RTMP
{

	RtmpConsumer::RtmpConsumer(RtmpConsumer::Listener* listener)
	  : listener_(listener), jitter_(new RtmpRtmpJitter())
	{
	}

	RtmpConsumer::~RtmpConsumer()
	{
		MS_DEBUG_DEV_STD("~RtmpConsumer");
		FREEP(jitter_);
		listener_ = nullptr;
	}

	// see RtmpRtmpConn::process_play_control_msg
	srs_error_t RtmpConsumer::RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		MS_DEBUG_DEV_STD("recv msg type is %d", msg->header.message_type);

		srs_error_t err = srs_success;

		if (!msg->header.is_amf0_command() && !msg->header.is_amf3_command())
		{
			return err;
		}

		RtmpPacket* pkt = NULL;
		if ((err = listener_->OnDecodeMessage(msg, &pkt)) != srs_success)
		{
			return srs_error_wrap(err, "rtmp: decode message");
		}
		RtmpAutoFree(RtmpPacket, pkt);

		// for jwplayer/flowplayer, which send close as pause message.
		RtmpCloseStreamPacket* close = dynamic_cast<RtmpCloseStreamPacket*>(pkt);
		if (close)
		{
			MS_DEBUG_DEV("RtmpCloseStreamPacket command_name=%s", close->command_name.c_str());
			// [dming] todo: 从router里删除
			// 晚点再实现，应该只需要关闭session即可。可以通知Listener，即Transport去处理
			return srs_error_new(ERROR_CONTROL_RTMP_CLOSE, "rtmp: close stream");
		}

		// call msg,
		// support response null first,
		// TODO: FIXME: response in right way, or forward in edge mode.
		RtmpCallPacket* call = dynamic_cast<RtmpCallPacket*>(pkt);
		if (call)
		{
			// only response it when transaction id not zero,
			// for the zero means donot need response.
			if (call->transaction_id > 0)
			{
				RtmpCallResPacket* res = new RtmpCallResPacket(call->transaction_id);
				res->command_object    = RtmpAmf0Any::null();
				res->response          = RtmpAmf0Any::null();
				if ((err = listener_->OnSendAndFreePacket(res, 0)) != srs_success)
				{
					return srs_error_wrap(err, "rtmp: send packets");
				}
			}
			return err;
		}

		// pause
		RtmpPausePacket* pause = dynamic_cast<RtmpPausePacket*>(pkt);
		if (pause)
		{
			if ((err = on_play_client_pause(pause->is_pause)) != srs_success)
			{
				return srs_error_wrap(err, "rtmp: pause");
			}
			return err;
		}

		return err;
	}

	srs_error_t RtmpConsumer::enqueue(
	  RtmpSharedPtrMessage* shared_msg, bool atc, RtmpRtmpJitterAlgorithm ag)
	{
		srs_error_t err = srs_success;

		RtmpSharedPtrMessage* msg = shared_msg->copy();

		if (!atc)
		{
			if ((err = jitter_->correct(msg, ag)) != srs_success)
			{
				return srs_error_wrap(err, "consume message");
			}
		}

		return this->listener_->OnSendAndFreeMessage(msg, listener_->GetInfo()->res->stream_id);
	}

	int64_t RtmpConsumer::get_time()
	{
		return jitter_->get_time();
	}

	srs_error_t RtmpConsumer::on_play_client_pause(bool is_pause)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV("stream consumer change pause state %d=>%d", b_paused_, is_pause);

		int stream_id = listener_->GetInfo()->res->stream_id;
		if (is_pause)
		{
			// onStatus(NetStream.Pause.Notify)
			if (true)
			{
				RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

				pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
				pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamPause));
				pkt->data->set(StatusDescription, RtmpAmf0Any::str("Paused stream."));

				if ((err = listener_->OnSendAndFreePacket(pkt, stream_id)) != srs_success)
				{
					return srs_error_wrap(err, "send NetStream.Pause.Notify");
				}
			}
			// StreamEOF
			if (true)
			{
				RtmpUserControlPacket* pkt = new RtmpUserControlPacket();

				pkt->event_type = SrcPCUCStreamEOF;
				pkt->event_data = stream_id;

				if ((err = listener_->OnSendAndFreePacket(pkt, 0)) != srs_success)
				{
					return srs_error_wrap(err, "send StreamEOF");
				}
			}
		}
		else
		{
			// onStatus(NetStream.Unpause.Notify)
			if (true)
			{
				RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

				pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
				pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamUnpause));
				pkt->data->set(StatusDescription, RtmpAmf0Any::str("Unpaused stream."));

				if ((err = listener_->OnSendAndFreePacket(pkt, stream_id)) != srs_success)
				{
					return srs_error_wrap(err, "send NetStream.Unpause.Notify");
				}
			}
			// StreamBegin
			if (true)
			{
				RtmpUserControlPacket* pkt = new RtmpUserControlPacket();

				pkt->event_type = SrcPCUCStreamBegin;
				pkt->event_data = stream_id;

				if ((err = listener_->OnSendAndFreePacket(pkt, 0)) != srs_success)
				{
					return srs_error_wrap(err, "send StreamBegin");
				}
			}
		}

		b_paused_ = is_pause;

		return err;
	}
} // namespace RTMP