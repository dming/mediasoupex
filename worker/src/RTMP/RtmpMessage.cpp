#define MS_CLASS "RTMP::RtmpMessage"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpMessage.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpUtility.hpp"

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
		payload = nullptr;
		size    = 0;
	}

	RtmpCommonMessage::~RtmpCommonMessage()
	{
		FREEPA(payload);
	}

	void RtmpCommonMessage::create_payload(int size)
	{
		FREEPA(payload);

		payload = new char[size];
	}

	srs_error_t RtmpCommonMessage::create(RtmpMessageHeader* pheader, char* body, int size)
	{
		// drop previous payload.
		FREEPA(payload);

		this->header  = *pheader;
		this->payload = body;
		this->size    = size;

		return srs_success;
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
		FREEPA(msg);
	}

	RtmpSharedMessageHeader::RtmpSharedMessageHeader()
	{
		payload_length = 0;
		message_type   = 0;
		perfer_cid     = 0;
	}

	RtmpSharedMessageHeader::~RtmpSharedMessageHeader()
	{
	}

	RtmpSharedPtrMessage::RtmpSharedPtrPayload::RtmpSharedPtrPayload()
	{
		payload      = nullptr;
		size         = 0;
		shared_count = 0;
	}

	RtmpSharedPtrMessage::RtmpSharedPtrPayload::~RtmpSharedPtrPayload()
	{
		FREEPA(payload);
	}

	RtmpSharedPtrMessage::RtmpSharedPtrMessage()
	  : timestamp(0), stream_id(0), size(0), payload(nullptr)
	{
		ptr = nullptr;

		// ++_srs_pps_objs_msgs->sugar;
	}

	RtmpSharedPtrMessage::~RtmpSharedPtrMessage()
	{
		if (ptr)
		{
			if (ptr->shared_count == 0)
			{
				FREEP(ptr);
			}
			else
			{
				ptr->shared_count--;
			}
		}
	}

	srs_error_t RtmpSharedPtrMessage::create(RtmpCommonMessage* msg)
	{
		srs_error_t err = srs_success;

		if ((err = create(&msg->header, msg->payload, msg->size)) != srs_success)
		{
			return srs_error_wrap(err, "create message");
		}

		// to prevent double free of payload:
		// initialize already attach the payload of msg,
		// detach the payload to transfer the owner to shared ptr.
		msg->payload = nullptr;
		msg->size    = 0;

		return err;
	}

	srs_error_t RtmpSharedPtrMessage::create(RtmpMessageHeader* pheader, char* payload, int size)
	{
		srs_error_t err = srs_success;

		if (size < 0)
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_CREATE, "create message size=%d", size);
		}

		srs_assert(!ptr);
		ptr = new RtmpSharedPtrPayload();

		// direct attach the data.
		if (pheader)
		{
			ptr->header.message_type   = pheader->message_type;
			ptr->header.payload_length = size;
			ptr->header.perfer_cid     = pheader->perfer_cid;
			this->timestamp            = pheader->timestamp;
			this->stream_id            = pheader->stream_id;
		}
		ptr->payload = payload;
		ptr->size    = size;

		// message can access it.
		this->payload = ptr->payload;
		this->size    = ptr->size;

		return err;
	}

	void RtmpSharedPtrMessage::wrap(char* payload, int size)
	{
		srs_assert(!ptr);
		ptr = new RtmpSharedPtrPayload();

		ptr->payload = payload;
		ptr->size    = size;

		this->payload = ptr->payload;
		this->size    = ptr->size;
	}

	int RtmpSharedPtrMessage::count()
	{
		return ptr ? ptr->shared_count : 0;
	}

	bool RtmpSharedPtrMessage::check(int stream_id)
	{
		// Ignore error when message has no payload.
		if (!ptr)
		{
			return true;
		}

		// we donot use the complex basic header,
		// ensure the basic header is 1bytes.
		if (ptr->header.perfer_cid < 2 || ptr->header.perfer_cid > 63)
		{
			MS_DEBUG_DEV_STD(
			  "change the chunk_id=%d to default=%d", ptr->header.perfer_cid, RTMP_CID_ProtocolControl);
			ptr->header.perfer_cid = RTMP_CID_ProtocolControl;
		}

		// we assume that the stream_id in a group must be the same.
		if (this->stream_id == stream_id)
		{
			return true;
		}
		this->stream_id = stream_id;

		return false;
	}

	bool RtmpSharedPtrMessage::is_av()
	{
		return ptr->header.message_type == RTMP_MSG_AudioMessage ||
		       ptr->header.message_type == RTMP_MSG_VideoMessage;
	}

	bool RtmpSharedPtrMessage::is_audio()
	{
		return ptr->header.message_type == RTMP_MSG_AudioMessage;
	}

	bool RtmpSharedPtrMessage::is_video()
	{
		return ptr->header.message_type == RTMP_MSG_VideoMessage;
	}

	int RtmpSharedPtrMessage::chunk_header(char* cache, int nb_cache, bool c0)
	{
		if (c0)
		{
			return srs_chunk_header_c0(
			  ptr->header.perfer_cid,
			  (uint32_t)timestamp,
			  ptr->header.payload_length,
			  ptr->header.message_type,
			  stream_id,
			  cache,
			  nb_cache);
		}
		else
		{
			return srs_chunk_header_c3(ptr->header.perfer_cid, (uint32_t)timestamp, cache, nb_cache);
		}
	}

	RtmpSharedPtrMessage* RtmpSharedPtrMessage::copy()
	{
		srs_assert(ptr);

		RtmpSharedPtrMessage* copy = copy2();

		copy->timestamp = timestamp;
		copy->stream_id = stream_id;

		return copy;
	}

	RtmpSharedPtrMessage* RtmpSharedPtrMessage::copy2()
	{
		RtmpSharedPtrMessage* copy = new RtmpSharedPtrMessage();

		// We got an object from cache, the ptr might exists, so unwrap it.
		// srs_assert(!copy->ptr);

		// Reference to this message instead.
		copy->ptr = ptr;
		ptr->shared_count++;

		copy->payload = ptr->payload;
		copy->size    = ptr->size;

		return copy;
	}
} // namespace RTMP