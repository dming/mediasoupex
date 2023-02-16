#define MS_CLASS "RTMP::RtmpMessage"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpMessage.hpp"
#include "Logger.hpp"

namespace RTMP
{

	// RtmpMessageHeader
	RtmpMessageHeader::RtmpMessageHeader()
	{
		MS_TRACE();
		message_type    = 0;
		payload_length  = 0;
		timestamp_delta = 0;
		stream_id       = 0;

		timestamp = 0;
		// we always use the connection chunk-id
		perfer_cid = RTMP_CID_OverConnection;
	}
	RtmpMessageHeader::~RtmpMessageHeader()
	{
		MS_TRACE();
	}
	bool RtmpMessageHeader::is_audio()
	{
		return message_type == RTMP_MSG_AudioMessage;
	}
	bool RtmpMessageHeader::is_video()
	{
		return message_type == RTMP_MSG_VideoMessage;
	}
	bool RtmpMessageHeader::is_amf0_command()
	{
		return message_type == RTMP_MSG_AMF0CommandMessage;
	}
	bool RtmpMessageHeader::is_amf0_data()
	{
		return message_type == RTMP_MSG_AMF0DataMessage;
	}
	bool RtmpMessageHeader::is_amf3_command()
	{
		return message_type == RTMP_MSG_AMF3CommandMessage;
	}
	bool RtmpMessageHeader::is_amf3_data()
	{
		return message_type == RTMP_MSG_AMF3DataMessage;
	}
	bool RtmpMessageHeader::is_window_ackledgement_size()
	{
		return message_type == RTMP_MSG_WindowAcknowledgementSize;
	}
	bool RtmpMessageHeader::is_ackledgement()
	{
		return message_type == RTMP_MSG_Acknowledgement;
	}
	bool RtmpMessageHeader::is_set_chunk_size()
	{
		return message_type == RTMP_MSG_SetChunkSize;
	}
	bool RtmpMessageHeader::is_user_control_message()
	{
		return message_type == RTMP_MSG_UserControlMessage;
	}
	bool RtmpMessageHeader::is_set_peer_bandwidth()
	{
		return message_type == RTMP_MSG_SetPeerBandwidth;
	}
	bool RtmpMessageHeader::is_aggregate()
	{
		return message_type == RTMP_MSG_AggregateMessage;
	}

	// Create a amf0 script header, set the size and stream_id.
	void RtmpMessageHeader::initialize_amf0_script(int size, int stream)
	{
		message_type    = RTMP_MSG_AMF0DataMessage;
		payload_length  = (int32_t)size;
		timestamp_delta = (int32_t)0;
		timestamp       = (int64_t)0;
		stream_id       = (int32_t)stream;

		// amf0 script use connection2 chunk-id
		perfer_cid = RTMP_CID_OverConnection2;
	}
	// Create a audio header, set the size, timestamp and stream_id.
	void RtmpMessageHeader::initialize_audio(int size, uint32_t time, int stream)
	{
		message_type    = RTMP_MSG_AudioMessage;
		payload_length  = (int32_t)size;
		timestamp_delta = (int32_t)time;
		timestamp       = (int64_t)time;
		stream_id       = (int32_t)stream;

		// audio chunk-id
		perfer_cid = RTMP_CID_Audio;
	}
	// Create a video header, set the size, timestamp and stream_id.
	void RtmpMessageHeader::initialize_video(int size, uint32_t time, int stream)
	{
		message_type    = RTMP_MSG_VideoMessage;
		payload_length  = (int32_t)size;
		timestamp_delta = (int32_t)time;
		timestamp       = (int64_t)time;
		stream_id       = (int32_t)stream;

		// video chunk-id
		perfer_cid = RTMP_CID_Video;
	}

	// RtmpCommonMessage
	RtmpCommonMessage::RtmpCommonMessage(/* args */)
	{
		payload = NULL;
		size    = 0;
	}

	RtmpCommonMessage::~RtmpCommonMessage()
	{
		FREEA(payload);
	}

	void RtmpCommonMessage::create_payload(int size)
	{
		FREEA(payload);

		payload = new char[size];
		MS_DEBUG_DEV("create payload for RTMP message. size=%d", size);
	}

	int RtmpCommonMessage::create(RtmpMessageHeader* pheader, char* body, int size)
	{
		// drop previous payload.
		FREEA(payload);

		this->header  = *pheader;
		this->payload = body;
		this->size    = size;

		return 0;
	}

	// RtmpChunkStream
	RtmpChunkStream::RtmpChunkStream(int _cid)
	{
		MS_TRACE();
		fmt                = 0;
		cid                = _cid;
		extended_timestamp = false;
		msg                = nullptr;
		msg_count          = 0;
	}
	RtmpChunkStream::~RtmpChunkStream()
	{
		FREEA(msg);
	}
} // namespace RTMP