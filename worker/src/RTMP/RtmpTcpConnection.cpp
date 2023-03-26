#define MS_CLASS "RTMP::RtmpTcpConnection"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpConnection.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpCommon.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "Utils.hpp"
#include <cstring> // std::memmove(), std::memcpy()

/**
 * whether always use complex send algorithm.
 * for some network does not support the complex send,
 * @see https://github.com/ossrs/srs/issues/320
 */
// #undef SRS_PERF_COMPLEX_SEND
#define SRS_PERF_COMPLEX_SEND

/**
 * 6.1.2. Chunk Message Header
 * There are four different formats for the chunk message header,
 * selected by the "fmt" field in the chunk basic header.
 */
// 6.1.2.1. Type 0
// Chunks of Type 0 are 11 bytes long. This type MUST be used at the
// start of a chunk stream, and whenever the stream timestamp goes
// backward (e.g., because of a backward seek).
#define RTMP_FMT_TYPE0 0
// 6.1.2.2. Type 1
// Chunks of Type 1 are 7 bytes long. The message stream ID is not
// included; this chunk takes the same stream ID as the preceding chunk.
// Streams with variable-sized messages (for example, many video
// formats) SHOULD use this format for the first chunk of each new
// message after the first.
#define RTMP_FMT_TYPE1 1
// 6.1.2.3. Type 2
// Chunks of Type 2 are 3 bytes long. Neither the stream ID nor the
// message length is included; this chunk has the same stream ID and
// message length as the preceding chunk. Streams with constant-sized
// messages (for example, some audio and data formats) SHOULD use this
// format for the first chunk of each message after the first.
#define RTMP_FMT_TYPE2 2
// 6.1.2.4. Type 3
// Chunks of Type 3 have no header. Stream ID, message length and
// timestamp delta are not present; chunks of this type take values from
// the preceding chunk. When a single message is split into chunks, all
// chunks of a message except the first one, SHOULD use this type. Refer
// to example 2 in section 6.2.2. Stream consisting of messages of
// exactly the same size, stream ID and spacing in time SHOULD use this
// type for all chunks after chunk of Type 2. Refer to example 1 in
// section 6.2.1. If the delta between the first message and the second
// message is same as the time stamp of first message, then chunk of
// type 3 would immediately follow the chunk of type 0 as there is no
// need for a chunk of type 2 to register the delta. If Type 3 chunk
// follows a Type 0 chunk, then timestamp delta for this Type 3 chunk is
// the same as the timestamp of Type 0 chunk.
#define RTMP_FMT_TYPE3 3

namespace RTMP
{
	RtmpTcpConnection::AckWindowSize::AckWindowSize()
	{
		window          = 0;
		sequence_number = 0;
		nb_recv_bytes   = 0;
	}

	RtmpTcpConnection::RtmpTcpConnection(Listener* listener, size_t bufferSize)
	  : ::TcpConnectionHandler::TcpConnectionHandler(bufferSize), listener(listener),
	    hsBytes(new RtmpHandshakeBytes()), in_chunk_size(SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE),
	    b_showDebugLog(true)
	{
		MS_TRACE();

		in_chunk_size    = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
		out_chunk_size   = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
		show_debug_info  = true;
		in_buffer_length = 0;

		nb_out_iovs = 8 * SRS_CONSTS_IOVS_MAX;
		out_iovs    = (uv_buf_t*)malloc(sizeof(uv_buf_t) * nb_out_iovs);

		warned_c0c3_cache_dry = false;

		chunkStreamCache = nullptr;
		if (SRS_PERF_CHUNK_STREAM_CACHE > 0)
		{
			chunkStreamCache = new RtmpChunkStream*[SRS_PERF_CHUNK_STREAM_CACHE];
		}
		for (int cid = 0; cid < SRS_PERF_CHUNK_STREAM_CACHE; cid++)
		{
			RtmpChunkStream* cs = new RtmpChunkStream(cid);
			// set the perfer cid of chunk,
			// which will copy to the message received.
			cs->header.perfer_cid = cid;

			chunkStreamCache[cid] = cs;
		}

		out_c0c3_caches = new char[SRS_CONSTS_C0C3_HEADERS_MAX];
	}

	RtmpTcpConnection::~RtmpTcpConnection()
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD(
		  "~RtmpTcpConnection Free ip:%s, port:%" PRIu16, GetPeerIp().c_str(), GetPeerPort());
		FREEP(hsBytes);

		// free all chunk stream cache.
		for (int i = 0; i < SRS_PERF_CHUNK_STREAM_CACHE; i++)
		{
			RtmpChunkStream* cs = chunkStreamCache[i];
			FREEP(cs);
		}
		FREEPA(chunkStreamCache);

		FREEPA(out_c0c3_caches);
	}

	void RtmpTcpConnection::OnRecvMessage(RtmpCommonMessage* msg)
	{
		srs_error_t err = srs_success;

		// try to response acknowledgement
		response_acknowledgement_message();

		RtmpPacket* packet = nullptr;
		switch (msg->header.message_type)
		{
			case RTMP_MSG_SetChunkSize:
			case RTMP_MSG_UserControlMessage:
			case RTMP_MSG_WindowAcknowledgementSize:
				if ((err = decode_message(msg, &packet)) != srs_success)
				{
					MS_ERROR("decode message fail");
					return;
				}
				break;
			case RTMP_MSG_VideoMessage:
			case RTMP_MSG_AudioMessage:
				print_debug_info();
			default:
				// 获得的msg直接抛给上层
				return this->listener->OnTcpConnectionPacketReceived(this, msg);
		}

		// always free the packet.
		RtmpAutoFree(RtmpPacket, packet);

		switch (msg->header.message_type)
		{
			case RTMP_MSG_WindowAcknowledgementSize:
			{
				RtmpSetWindowAckSizePacket* pkt = dynamic_cast<RtmpSetWindowAckSizePacket*>(packet);
				srs_assert(pkt != NULL);
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpSetWindowAckSizePacket ");

				if (pkt->ackowledgement_window_size > 0)
				{
					in_ack_size.window = (uint32_t)pkt->ackowledgement_window_size;
					// @remark, we ignore this message, for user noneed to care.
					// but it's important for dev, for client/server will block if required
					// ack msg not arrived.
				}
				break;
			}
			case RTMP_MSG_SetChunkSize:
			{
				RtmpSetChunkSizePacket* pkt = dynamic_cast<RtmpSetChunkSizePacket*>(packet);
				srs_assert(pkt != NULL);
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpSetChunkSizePacket ");

				// for some server, the actual chunk size can greater than the max value(65536),
				// so we just warning the invalid chunk size, and actually use it is ok,
				// @see: https://github.com/ossrs/srs/issues/160
				if (pkt->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE || pkt->chunk_size > SRS_CONSTS_RTMP_MAX_CHUNK_SIZE)
				{
					MS_WARN_DEV(
					  "accept chunk=%d, should in [%d, %d], please see #160",
					  pkt->chunk_size,
					  SRS_CONSTS_RTMP_MIN_CHUNK_SIZE,
					  SRS_CONSTS_RTMP_MAX_CHUNK_SIZE);
				}

				// @see: https://github.com/ossrs/srs/issues/541
				if (pkt->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE)
				{
					MS_ERROR(
					  "chunk size should be %d+, value=%d", SRS_CONSTS_RTMP_MIN_CHUNK_SIZE, pkt->chunk_size);
					return;
				}

				in_chunk_size = pkt->chunk_size;
				break;
			}
			case RTMP_MSG_UserControlMessage:
			{
				RtmpUserControlPacket* pkt = dynamic_cast<RtmpUserControlPacket*>(packet);
				srs_assert(pkt != NULL);
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpUserControlPacket ");

				if (pkt->event_type == SrcPCUCSetBufferLength)
				{
					in_buffer_length = pkt->extra_data;
				}
				if (pkt->event_type == SrcPCUCPingRequest)
				{
					if ((err = response_ping_message(pkt->event_data)) != srs_success)
					{
						MS_ERROR("response ping");
						return;
					}
				}
				break;
			}
			default:
				break;
		}
		return;
	}

	/**
	 * 每次收到数据都会调用UserOnTcpConnectionRead
	 * handshake 数据也应该在这里实现，传入this.buffer* 及 recvBytes*即可在外部读取handshake字节。
	 */
	void RtmpTcpConnection::UserOnTcpConnectionRead()
	{
		// static uint32_t times = 0;
		MS_TRACE();

		// if (b_showDebugLog)
		// {
		// 	MS_DEBUG_DEV_STD(
		// 	  "data received [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "] unRead:%" PRIu64
		// 	  ", times:%" PRIu32 ".",
		// 	  GetLocalIp().c_str(),
		// 	  GetLocalPort(),
		// 	  GetPeerIp().c_str(),
		// 	  GetPeerPort(),
		// 	  (size_t)(this->bufferDataLen - this->frameStart),
		// 	  times++);
		// }

		/**
		 * if !handshake then run Handshake.class until handshake done.
		 */
		if (!hsBytes->done)
		{
			RtmpHandshake handshake;
			handshake.HandshakeWithClient(hsBytes, this);
		}
		else
		{
			/**
			 * read rtmp tcp pecket;
			 */
			while (true) // true
			{
				if (IsClosed())
					return;

				/**
				 * 需要先解析出header，再根据header去解析出body
				 */
				size_t dataLen = this->bufferDataLen - this->frameStart;
				if (dataLen == 0)
					return AfterUserRead("no more free message space");

				RtmpCommonMessage* msg = nullptr;
				if (RecvInterlacedMessage(&msg) != srs_success)
				{
					FREEP(msg);
					if (b_showDebugLog)
					{
						MS_DEBUG_DEV_STD("recv interlaced message");
					}
					return AfterUserRead("recv interlaced message");
				}

				if (!msg)
				{
					continue;
				}

				if (msg->size <= 0 || msg->header.payload_length <= 0)
				{
					if (b_showDebugLog)
					{
						MS_DEBUG_DEV_STD(
						  "ignore empty message(type=%d, size=%d, time=%" PRId64 ", sid=%d).",
						  msg->header.message_type,
						  msg->header.payload_length,
						  msg->header.timestamp,
						  msg->header.stream_id);
					}
					FREEP(msg);
				}
				else
				{
					RtmpAutoFree(RtmpCommonMessage, msg);
					OnRecvMessage(msg);
				}

				// If there is more data in the buffer after the parsed frame then
				// parse again. Otherwise break here and wait for more data.
				if (this->bufferDataLen > this->frameStart)
				{
					// MS_DEBUG_DEV("there is more data after the parsed frame, continue parsing");
					continue;
				}

				return AfterUserRead("recv entire message");
			}
		}
	}

	int RtmpTcpConnection::ReadFully(char* data, size_t dataSize)
	{
		MS_TRACE();
		size_t dataLen = this->bufferDataLen - this->frameStart;
		if (dataLen < dataSize)
		{
			if (b_showDebugLog)
			{
				MS_ERROR_STD("not enought data for size %" PRIu64 ", only have %" PRIu64, dataSize, dataLen);
			}
			return -1;
		}

		const uint8_t* packet = this->buffer + this->frameStart;
		std::memcpy(data, packet, dataSize);

		this->frameStart += dataSize;
		AfterUserRead("ReadFully");

		return 0;
	}

	void RtmpTcpConnection::Send(
	  const uint8_t* data, size_t len, ::TcpConnectionHandler::onSendCallback* cb)
	{
		MS_TRACE();
		::TcpConnectionHandler::Write(data, len, cb);
	}

	srs_error_t RtmpTcpConnection::RecvInterlacedMessage(RtmpCommonMessage** pmsg)
	{
		MS_TRACE();
		srs_error_t err = srs_success;
		// chunk stream basic header.
		char fmt  = 0;
		int cid   = 0;
		int bhLen = 0;
		if ((err = ReadBasicHeader(fmt, cid, bhLen)) != 0)
		{
			MS_DEBUG_DEV_STD("read basic header wrong");
			return err;
		}

		// the cid must not negative.
		MS_ASSERT(cid >= 0, "the cid must not negative.");
		if (b_showDebugLog)
		{
			MS_DEBUG_DEV_STD("cid is %d", cid);
		}

		// get the cached chunk stream.
		RtmpChunkStream* chunk = nullptr;

		// use chunk stream cache to get the chunk info.
		// @see https://github.com/ossrs/srs/issues/249
		if (cid < SRS_PERF_CHUNK_STREAM_CACHE)
		{
			// already init, use it direclty
			chunk = chunkStreamCache[cid];
		}
		else
		{
			// chunk stream cache miss, use map.
			if (chunkStreams.find(cid) == chunkStreams.end())
			{
				chunk = chunkStreams[cid] = new RtmpChunkStream(cid);
				// set the perfer cid of chunk,
				// which will copy to the message received.
				chunk->header.perfer_cid = cid;
			}
			else
			{
				chunk = chunkStreams[cid];
			}
		}

		if ((err = ReadMessageHeader(chunk, fmt, bhLen)) != srs_success)
		{
			if (b_showDebugLog)
			{
				MS_ERROR_STD("read message header fail");
			}
			return err;
		}

		// read msg payload from chunk stream.
		RtmpCommonMessage* msg = nullptr;
		if ((err = ReadMessagePayload(chunk, &msg)) != srs_success) // [dming] third: read full body
		{
			if (b_showDebugLog)
			{
				MS_ERROR_STD("read message payload");
			}
			return err;
		}

		// not got an entire RTMP message, try next chunk.
		if (!msg)
		{
			return err;
		}

		*pmsg = msg;
		return err;
	}

	/**
	 * 6.1.1. Chunk Basic Header
	 * The Chunk Basic Header encodes the chunk stream ID and the chunk
	 * type(represented by fmt field in the figure below). Chunk type
	 * determines the format of the encoded message header. Chunk Basic
	 * Header field may be 1, 2, or 3 bytes, depending on the chunk stream
	 * ID.
	 *
	 * The bits 0-5 (least significant) in the chunk basic header represent
	 * the chunk stream ID.
	 *
	 * Chunk stream IDs 2-63 can be encoded in the 1-byte version of this
	 * field.
	 *    0 1 2 3 4 5 6 7
	 *   +-+-+-+-+-+-+-+-+
	 *   |fmt|   cs id   |
	 *   +-+-+-+-+-+-+-+-+
	 *   Figure 6 Chunk basic header 1
	 *
	 * Chunk stream IDs 64-319 can be encoded in the 2-byte version of this
	 * field. ID is computed as (the second byte + 64).
	 *   0                   1
	 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
	 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *   |fmt|    0      | cs id - 64    |
	 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *   Figure 7 Chunk basic header 2
	 *
	 * Chunk stream IDs 64-65599 can be encoded in the 3-byte version of
	 * this field. ID is computed as ((the third byte)*256 + the second byte
	 * + 64).
	 *    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
	 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *   |fmt|     1     |         cs id - 64            |
	 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
	 *   Figure 8 Chunk basic header 3
	 *
	 * cs id: 6 bits
	 * fmt: 2 bits
	 * cs id - 64: 8 or 16 bits
	 *
	 * Chunk stream IDs with values 64-319 could be represented by both 2-
	 * byte version and 3-byte version of this field.
	 */
	srs_error_t RtmpTcpConnection::ReadBasicHeader(char& fmt, int& cid, int& bhLen)
	{
		// 只要读完Header，就可以知道这个message所需的所有字节数，就知道能否读出完整的chunk

		/**
		 * 1. 首先读完baseHeader, 得到fmt以及cid，还有读取了多少字节
		 * 2. 继续读headerMessage, 得到payload，这时候就可以得到payload size，根据fmt对应的header size
		 * 以及是否存在external timestamp, 先行判断是否可以在该循环里读完整个chunk
		 * 3. 如果无法读完整个chunk，放弃读取，将buffer重置。
		 */

		size_t dataLen = this->bufferDataLen - this->frameStart;
		if (dataLen < 1)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV_STD("basic header requires at least 1 bytes");
			}
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "basic header requires 1 bytes");
		}

		const char* p = (char*)(this->buffer + this->frameStart);
		fmt           = *p++;
		cid           = fmt & 0x3f;
		fmt           = (fmt >> 6) & 0x03;

		// 2-63, 1B chunk header
		if (cid > 1)
		{
			bhLen = 1;
		}
		// 64-319, 2B chunk header
		else if (cid == 0)
		{
			if (dataLen < 2)
			{
				if (b_showDebugLog)
				{
					MS_DEBUG_DEV_STD("basic header requires 2 bytes");
				}
				return srs_error_new(ERROR_RTMP_SOUP_ERROR, "basic header requires 2 bytes");
			}
			cid = 64;
			cid += (uint8_t)*p++;
			bhLen = 2;
		}
		// 64-65599, 3B chunk header
		else
		{
			MS_ASSERT(cid == 1, "cid must be 1");
			if (dataLen < 3)
			{
				if (b_showDebugLog)
				{
					MS_DEBUG_DEV_STD("basic header requires 3 bytes");
				}
				return srs_error_new(ERROR_RTMP_SOUP_ERROR, "basic header requires 3 bytes");
			}
			cid = 64;
			cid += (uint8_t)*p++;
			cid += ((uint8_t)*p++) * 256;
			bhLen = 3;
		}

		return srs_success;
	}

	srs_error_t RtmpTcpConnection::ReadMessageHeader(RtmpChunkStream* chunk, char fmt, int bhLen)
	{
		static char MH_SIZE[] = { 11, 7, 3, 0 };
		/**
		 * parse the message header.
		 *   3bytes: timestamp delta,    fmt=0,1,2
		 *   3bytes: payload length,     fmt=0,1
		 *   1bytes: message type,       fmt=0,1
		 *   4bytes: stream id,          fmt=0
		 */
		struct MessageHeader
		{
			uint8_t timestampDelta[3];
			uint8_t payloadLength[3];
			uint8_t messageType;
			uint8_t streamId[4]; /* 小端格式 */
		};

		uint32_t chunk_timestamp;

		/**
		 * we should not assert anything about fmt, for the first packet.
		 * (when first packet, the chunk->msg is nullptr).
		 * the fmt maybe 0/1/2/3, the FMLE will send a 0xC4 for some audio packet.
		 * the previous packet is:
		 *     04                // fmt=0, cid=4
		 *     00 00 1a          // timestamp=26
		 *     00 00 9d          // payload_length=157
		 *     08                // message_type=8(audio)
		 *     01 00 00 00       // stream_id=1
		 * the current packet maybe:
		 *     c4             // fmt=3, cid=4
		 * it's ok, for the packet is audio, and timestamp delta is 26.
		 * the current packet must be parsed as:
		 *     fmt=0, cid=4
		 *     timestamp=26+26=52
		 *     payload_length=157
		 *     message_type=8(audio)
		 *     stream_id=1
		 * so we must update the timestamp even fmt=3 for first packet.
		 */
		// fresh packet used to update the timestamp even fmt=3 for first packet.
		// fresh packet always means the chunk is the first one of message.
		bool is_first_chunk_of_msg = !chunk->msg;

		// but, we can ensure that when a chunk stream is fresh,
		// the fmt must be 0, a new stream.
		if (chunk->msg_count == 0 && fmt != RTMP_FMT_TYPE0)
		{
			// for librtmp, if ping, it will send a fresh stream with fmt=1,
			// 0x42             where: fmt=1, cid=2, protocol contorl user-control message
			// 0x00 0x00 0x00   where: timestamp=0
			// 0x00 0x00 0x06   where: payload_length=6
			// 0x04             where: message_type=4(protocol control user-control message)
			// 0x00 0x06            where: event Ping(0x06)
			// 0x00 0x00 0x0d 0x0f  where: event data 4bytes ping timestamp.
			if (fmt == RTMP_FMT_TYPE1)
			{
				MS_WARN_DEV("fresh chunk starts with fmt=1");
			}
			else
			{
				// must be a RTMP protocol level error.
				// return srs_error_new(
				//   ERROR_RTMP_CHUNK_START, "fresh chunk expect fmt=0, actual=%d, cid=%d", fmt, chunk->cid);
				MS_ERROR_STD("fresh chunk expect fmt=0, actual=%d, cid=%d", fmt, chunk->cid);
				// must be a RTMP protocol level error.
				return srs_error_new(
				  ERROR_RTMP_CHUNK_START, "fresh chunk expect fmt=0, actual=%d, cid=%d", fmt, chunk->cid);
			}
		}

		// when exists cache msg, means got an partial message,
		// the fmt must not be type0 which means new message.
		if (chunk->msg && fmt == RTMP_FMT_TYPE0)
		{
			// return srs_error_new(ERROR_RTMP_CHUNK_START, "for existed chunk, fmt should not be 0");
			MS_ERROR_STD("for existed chunk, fmt should not be 0");
			return srs_error_new(ERROR_RTMP_CHUNK_START, "for existed chunk, fmt should not be 0");
		}

		// create msg when new chunk stream start
		if (!chunk->msg)
		{
			chunk->msg = new RtmpCommonMessage();
		}

		int mhSize     = MH_SIZE[(int)fmt];
		size_t dataLen = this->bufferDataLen - this->frameStart;
		if (mhSize > 0 && dataLen < bhLen + mhSize)
		{
			if (b_showDebugLog)
			{
				MS_ERROR_STD("cannot read %" PRId32 " bytes message header", bhLen + mhSize);
			}
			goto NOT_ENOUGH_DATA;
		}

		MessageHeader _header;
		memset(&_header, 0, sizeof(MessageHeader));
		memcpy(&_header, this->buffer + this->frameStart + bhLen, mhSize);

		/**
		 * parse the message header.
		 *   3bytes: timestamp delta,    fmt=0,1,2
		 *   3bytes: payload length,     fmt=0,1
		 *   1bytes: message type,       fmt=0,1
		 *   4bytes: stream id,          fmt=0
		 * where:
		 *   fmt=0, 0x0X
		 *   fmt=1, 0x4X
		 *   fmt=2, 0x8X
		 *   fmt=3, 0xCX
		 */
		// see also: ngx_rtmp_recv
		if (fmt <= RTMP_FMT_TYPE2)
		{
			chunk->header.timestamp_delta =
			  Utils::Byte::ReadUint24BE((char*)_header.timestampDelta); // 改成大端序
			bool b_extenedTimestamp = (chunk->header.timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);
			if (b_extenedTimestamp)
			{
				mhSize += 4;
				if (dataLen < bhLen + mhSize)
				{
					if (b_showDebugLog)
					{
						MS_ERROR_STD("cannot read %" PRId32 " bytes message header", bhLen + mhSize);
					}
					goto NOT_ENOUGH_DATA;
				}
			}
			int32_t fullPayloadLength = chunk->header.payload_length;
			if (fmt <= RTMP_FMT_TYPE1)
			{
				fullPayloadLength = Utils::Byte::ReadUint24BE((char*)_header.payloadLength);
			}
			int payloadSize = fullPayloadLength - chunk->msg->size;
			payloadSize     = std::min(payloadSize, in_chunk_size);
			if (payloadSize > 0 && dataLen < bhLen + mhSize + payloadSize)
			{
				if (b_showDebugLog)
				{
					MS_ERROR_STD(
					  "fmt=%d, cannot read %d bytes message header, and %d bytes payload size, only has %" PRIu64,
					  fmt,
					  bhLen + mhSize,
					  payloadSize,
					  dataLen);
				}
				goto NOT_ENOUGH_DATA;
			}

			// fmt: 0
			// timestamp: 3 bytes
			// If the timestamp is greater than or equal to 16777215
			// (hexadecimal 0x00ffffff), this value MUST be 16777215, and the
			// 'extended timestamp header' MUST be present. Otherwise, this value
			// SHOULD be the entire timestamp.
			//
			// fmt: 1 or 2
			// timestamp delta: 3 bytes
			// If the delta is greater than or equal to 16777215 (hexadecimal
			// 0x00ffffff), this value MUST be 16777215, and the 'extended
			// timestamp header' MUST be present. Otherwise, this value SHOULD be
			// the entire delta.
			chunk->extended_timestamp = b_extenedTimestamp;
			if (!chunk->extended_timestamp)
			{
				// Extended timestamp: 0 or 4 bytes
				// This field MUST be sent when the normal timsestamp is set to
				// 0xffffff, it MUST NOT be sent if the normal timestamp is set to
				// anything else. So for values less than 0xffffff the normal
				// timestamp field SHOULD be used in which case the extended timestamp
				// MUST NOT be present. For values greater than or equal to 0xffffff
				// the normal timestamp field MUST NOT be used and MUST be set to
				// 0xffffff and the extended timestamp MUST be sent.
				if (fmt == RTMP_FMT_TYPE0)
				{
					// 6.1.2.1. Type 0
					// For a type-0 chunk, the absolute timestamp of the message is sent
					// here.
					chunk->header.timestamp = chunk->header.timestamp_delta;
				}
				else
				{
					// 6.1.2.2. Type 1
					// 6.1.2.3. Type 2
					// For a type-1 or type-2 chunk, the difference between the previous
					// chunk's timestamp and the current chunk's timestamp is sent here.
					chunk->header.timestamp += chunk->header.timestamp_delta;
				}
			}

			if (fmt <= RTMP_FMT_TYPE1)
			{
				// for a message, if msg exists in cache, the size must not changed.
				// always use the actual msg size to compare, for the cache payload length can changed,
				// for the fmt type1(stream_id not changed), user can change the payload
				// length(it's not allowed in the continue chunks).
				if (!is_first_chunk_of_msg && chunk->header.payload_length != fullPayloadLength)
				{
					MS_ERROR_STD(
					  "msg in chunk cache, size=%d cannot change to %d",
					  chunk->header.payload_length,
					  fullPayloadLength);
					return srs_error_new(
					  ERROR_RTMP_PACKET_SIZE,
					  "msg in chunk cache, size=%d cannot change to %d",
					  chunk->header.payload_length,
					  fullPayloadLength);
				}

				chunk->header.payload_length = fullPayloadLength;
				chunk->header.message_type   = _header.messageType;

				if (fmt == RTMP_FMT_TYPE0)
				{
					chunk->header.stream_id = Utils::Byte::ReadUint24LE((char*)_header.streamId);
				}
			}
		}
		else
		{
			int payloadSize = chunk->header.payload_length - chunk->msg->size;
			payloadSize     = std::min(payloadSize, in_chunk_size);
			if (payloadSize > 0 && dataLen < (bhLen + mhSize + payloadSize))
			{
				if (b_showDebugLog)
				{
					MS_ERROR_STD(
					  "fmt=%d, cannot read %d bytes message header, and %d bytes payload size, only %" PRIu64,
					  fmt,
					  bhLen + mhSize,
					  chunk->header.payload_length - chunk->msg->size,
					  dataLen);
				}
				goto NOT_ENOUGH_DATA;
			}
			// update the timestamp even fmt=3 for first chunk packet
			if (is_first_chunk_of_msg && !chunk->extended_timestamp)
			{
				chunk->header.timestamp += chunk->header.timestamp_delta;
			}
		}

		// read extended-timestamp
		if (chunk->extended_timestamp)
		{
			uint32_t timestamp =
			  Utils::Byte::ReadUint32BE((char*)this->buffer + this->frameStart + bhLen + mhSize);

			// always use 31bits timestamp, for some server may use 32bits extended timestamp.
			timestamp &= 0x7fffffff;

			/**
			 * RTMP specification and ffmpeg/librtmp is false,
			 * but, adobe changed the specification, so flash/FMLE/FMS always true.
			 * default to true to support flash/FMLE/FMS.
			 *
			 * ffmpeg/librtmp may donot send this filed, need to detect the value.
			 * @see also: http://blog.csdn.net/win_lin/article/details/13363699
			 * compare to the chunk timestamp, which is set by chunk message header
			 * type 0,1 or 2.
			 *
			 * @remark, nginx send the extended-timestamp in sequence-header,
			 * and timestamp delta in continue C1 chunks, and so compatible with ffmpeg,
			 * that is, there is no continue chunks and extended-timestamp in nginx-rtmp.
			 *
			 * @remark, srs always send the extended-timestamp, to keep simple,
			 * and compatible with adobe products.
			 */
			chunk_timestamp = (uint32_t)chunk->header.timestamp;

			/**
			 * if chunk_timestamp<=0, the chunk previous packet has no extended-timestamp,
			 * always use the extended timestamp.
			 */
			/**
			 * about the is_first_chunk_of_msg.
			 * @remark, for the first chunk of message, always use the extended timestamp.
			 */
			if (!is_first_chunk_of_msg && chunk_timestamp > 0 && chunk_timestamp != timestamp)
			{
				mhSize -= 4;
			}
			else
			{
				chunk->header.timestamp = timestamp;
			}
		}
		// the extended-timestamp must be unsigned-int,
		//         24bits timestamp: 0xffffff = 16777215ms = 16777.215s = 4.66h
		//         32bits timestamp: 0xffffffff = 4294967295ms = 4294967.295s = 1193.046h = 49.71d
		// because the rtmp protocol says the 32bits timestamp is about "50 days":
		//         3. Byte Order, Alignment, and Time Format
		//                Because timestamps are generally only 32 bits long, they will roll
		//                over after fewer than 50 days.
		//
		// but, its sample says the timestamp is 31bits:
		//         An application could assume, for example, that all
		//        adjacent timestamps are within 2^31 milliseconds of each other, so
		//        10000 comes after 4000000000, while 3000000000 comes before
		//        4000000000.
		// and flv specification says timestamp is 31bits:
		//        Extension of the Timestamp field to form a SI32 value. This
		//        field represents the upper 8 bits, while the previous
		//        Timestamp field represents the lower 24 bits of the time in
		//        milliseconds.
		// in a word, 31bits timestamp is ok.
		// convert extended timestamp to 31bits.
		chunk->header.timestamp &= 0x7fffffff;

		// valid message, the payload_length is 24bits,
		// so it should never be negative.
		MS_ASSERT(chunk->header.payload_length >= 0, "chunk header payload length must be positive.");

		// copy header to msg
		chunk->msg->header = chunk->header;

		// increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
		chunk->msg_count++;

		this->frameStart += bhLen + mhSize;

		return srs_success;

	NOT_ENOUGH_DATA:
		if (chunk->msg && chunk->msg->size == 0)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV_STD("delete chunk->msg");
			}
			FREEP(chunk->msg);
			chunk->msg = nullptr;
		}

		if (b_showDebugLog)
		{
			MS_DEBUG_DEV_STD("frame not finished yet, waiting for more data");
		}
		return srs_error_new(ERROR_RTMP_SOUP_ERROR, "not enough data for message");
	}

	srs_error_t RtmpTcpConnection::ReadMessagePayload(RtmpChunkStream* chunk, RtmpCommonMessage** pmsg)
	{
		MS_TRACE();

		srs_error_t err = srs_success;
		// empty message
		if (chunk->header.payload_length <= 0)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV_STD(
				  "get an empty RTMP message(type=%d, size=%d, time=%" PRId64 ", sid=%d)",
				  chunk->header.message_type,
				  chunk->header.payload_length,
				  chunk->header.timestamp,
				  chunk->header.stream_id);
			}

			*pmsg      = chunk->msg;
			chunk->msg = nullptr;

			return err;
		}

		MS_ASSERT(chunk->header.payload_length > 0, "chunk header payload length must be positive.");

		// the chunk payload size.
		int payload_size = chunk->header.payload_length - chunk->msg->size;
		payload_size     = std::min(payload_size, in_chunk_size);

		// create msg payload if not initialized
		if (!chunk->msg->payload)
		{
			chunk->msg->create_payload(chunk->header.payload_length);
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV_STD("create payload for RTMP message. size=%d", chunk->header.payload_length);
			}
		}
		memcpy(chunk->msg->payload + chunk->msg->size, this->buffer + this->frameStart, payload_size);
		chunk->msg->size += payload_size;

		if (b_showDebugLog)
		{
			MS_DEBUG_DEV_STD(
			  "chunk msg size is :%d, and payload length is %d",
			  chunk->msg->size,
			  chunk->header.payload_length);
		}

		this->frameStart += payload_size;

		// got entire RTMP message?
		if (chunk->header.payload_length == chunk->msg->size)
		{
			*pmsg      = chunk->msg;
			chunk->msg = nullptr;
			return err;
		}

		return err;
	}

	void RtmpTcpConnection::AfterUserRead(std::string log)
	{
		// Check if the buffer is full.
		if (this->bufferDataLen == this->bufferSize)
		{
			// First case: the incomplete frame does not begin at position 0 of
			// the buffer, so move the frame to the position 0.
			if (this->frameStart != 0)
			{
				std::memmove(
				  this->buffer, this->buffer + this->frameStart, this->bufferSize - this->frameStart);
				this->bufferDataLen = this->bufferSize - this->frameStart;
				this->frameStart    = 0;

				if (b_showDebugLog)
				{
					MS_DEBUG_DEV_STD(
					  "no more space in the buffer, moving parsed bytes to the beginning of the buffer and wait for more data. bufferDataLen=%" PRIu64
					  ", remainSize=%" PRIu64 ", log=%s",
					  this->bufferDataLen,
					  this->bufferSize - this->bufferDataLen,
					  log.c_str());
				}
			}
			// Second case: the incomplete frame begins at position 0 of the buffer.
			// The frame is too big.
			else
			{
				MS_WARN_DEV_STD(
				  "no more space in the buffer for the unfinished frame being parsed, closing the "
				  "connection.. log=%s",
				  log.c_str());

				ErrorReceiving();

				// And exit fast since we are supposed to be deallocated.
				return;
			}
		}
		// The buffer is not full.
	}

	srs_error_t RtmpTcpConnection::send_and_free_packet(RtmpPacket* packet, int stream_id)
	{
		srs_error_t err = srs_success;

		if ((err = do_send_and_free_packet(packet, stream_id)) != srs_success)
		{
			return srs_error_wrap(err, "send packet");
		}

		// // flush messages in manual queue
		// if ((err = manual_response_flush()) != srs_success) //[dming] TODO:先忽略，后面再补
		// {
		// 	return srs_error_wrap(err, "manual flush response");
		// }

		return err;
	}

	srs_error_t RtmpTcpConnection::do_send_and_free_packet(RtmpPacket* packet, int stream_id)
	{
		srs_error_t err = srs_success;

		srs_assert(packet);
		RtmpAutoFree(RtmpPacket, packet);
		RtmpCommonMessage* msg = new RtmpCommonMessage();
		RtmpAutoFree(RtmpCommonMessage, msg);

		if ((err = packet->to_msg(msg, stream_id)) != srs_success)
		{
			return srs_error_wrap(err, "to message");
		}

		RtmpSharedPtrMessage* shared_msg = new RtmpSharedPtrMessage();
		if ((err = shared_msg->create(msg)) != srs_success)
		{
			FREEP(shared_msg);
			return srs_error_wrap(err, "create message");
		}

		if ((err = send_and_free_message(shared_msg, stream_id)) != srs_success)
		{
			return srs_error_wrap(err, "send packet");
		}

		if ((err = on_send_packet(&msg->header, packet)) != srs_success)
		{
			return srs_error_wrap(err, "on send packet");
		}

		return err;
	}

	srs_error_t RtmpTcpConnection::send_and_free_message(RtmpSharedPtrMessage* msg, int stream_id)
	{
		return send_and_free_messages(&msg, 1, stream_id);
	}

	srs_error_t RtmpTcpConnection::send_and_free_messages(
	  RtmpSharedPtrMessage** msgs, int nb_msgs, int stream_id)
	{
		// always not nullptr msg.
		srs_assert(msgs);
		srs_assert(nb_msgs > 0);

		// update the stream id in header.
		for (int i = 0; i < nb_msgs; i++)
		{
			RtmpSharedPtrMessage* msg = msgs[i];

			if (!msg)
			{
				continue;
			}

			// check perfer cid and stream,
			// when one msg stream id is ok, ignore left.
			if (msg->check(stream_id))
			{
				break;
			}
		}

		// donot use the auto free to free the msg,
		// for performance issue.
		srs_error_t err = do_send_messages(msgs, nb_msgs);

		for (int i = 0; i < nb_msgs; i++)
		{
			RtmpSharedPtrMessage* msg = msgs[i];
			FREEP(msg);
		}

		// donot flush when send failed
		if (err != srs_success)
		{
			return srs_error_wrap(err, "send messages");
		}

		// // flush messages in manual queue
		// if ((err = manual_response_flush()) != srs_success)
		// {
		// 	return srs_error_wrap(err, "manual flush response");
		// }

		// print_debug_info();

		return err;
	}

	srs_error_t RtmpTcpConnection::do_send_messages(RtmpSharedPtrMessage** msgs, int nb_msgs)
	{
		srs_error_t err = srs_success;

#ifdef SRS_PERF_COMPLEX_SEND
		int iov_index  = 0;
		uv_buf_t* iovs = out_iovs + iov_index;

		int c0c3_cache_index = 0;
		char* c0c3_cache     = out_c0c3_caches + c0c3_cache_index;

		// try to send use the c0c3 header cache,
		// if cache is consumed, try another loop.
		for (int i = 0; i < nb_msgs; i++)
		{
			RtmpSharedPtrMessage* msg = msgs[i];

			if (!msg)
			{
				continue;
			}

			// ignore empty message.
			if (!msg->payload || msg->size <= 0)
			{
				continue;
			}

			// p set to current write position,
			// it's ok when payload is nullptr and size is 0.
			char* p    = msg->payload;
			char* pend = msg->payload + msg->size;

			// always write the header event payload is empty.
			while (p < pend)
			{
				// always has header
				int nb_cache = SRS_CONSTS_C0C3_HEADERS_MAX - c0c3_cache_index;
				int nbh      = msg->chunk_header(c0c3_cache, nb_cache, p == msg->payload);
				srs_assert(nbh > 0);

				// header iov
				iovs[0].base = c0c3_cache;
				iovs[0].len  = nbh;

				// payload iov
				int payload_size = std::min(out_chunk_size, (int)(pend - p));
				iovs[1].base     = p;
				iovs[1].len      = payload_size;

				// MS_DEBUG_DEV_STD("add header size=%d, payload size=%d", nbh, payload_size);

				// consume sendout bytes.
				p += payload_size;

				// realloc the iovs if exceed,
				// for we donot know how many messges maybe to send entirely,
				// we just alloc the iovs, it's ok.
				if (iov_index >= nb_out_iovs - 2)
				{
					int ov           = nb_out_iovs;
					nb_out_iovs      = 2 * nb_out_iovs;
					int realloc_size = sizeof(uv_buf_t) * nb_out_iovs;
					out_iovs         = (uv_buf_t*)realloc(out_iovs, realloc_size);
					MS_WARN_DEV("resize iovs %d => %d, max_msgs=%d", ov, nb_out_iovs, SRS_PERF_MW_MSGS);
				}

				// to next pair of iovs
				iov_index += 2;
				iovs = out_iovs + iov_index;

				// to next c0c3 header cache
				c0c3_cache_index += nbh;
				c0c3_cache = out_c0c3_caches + c0c3_cache_index;

				// the cache header should never be realloc again,
				// for the ptr is set to iovs, so we just warn user to set larger
				// and use another loop to send again.
				int c0c3_left = SRS_CONSTS_C0C3_HEADERS_MAX - c0c3_cache_index;
				if (c0c3_left < SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE)
				{
					// only warn once for a connection.
					if (!warned_c0c3_cache_dry)
					{
						MS_WARN_DEV(
						  "c0c3 cache header too small, recoment to %d",
						  SRS_CONSTS_C0C3_HEADERS_MAX + SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE);
						warned_c0c3_cache_dry = true;
					}

					// when c0c3 cache dry,
					// sendout all messages and reset the cache, then send again.
					if ((err = do_iovs_send(out_iovs, iov_index)) != srs_success)
					{
						return srs_error_wrap(err, "send iovs");
					}

					// reset caches, while these cache ensure
					// atleast we can sendout a chunk.
					iov_index = 0;
					iovs      = out_iovs + iov_index;

					c0c3_cache_index = 0;
					c0c3_cache       = out_c0c3_caches + c0c3_cache_index;
				}
			}
		}

		// maybe the iovs already sendout when c0c3 cache dry,
		// so just ignore when no iovs to send.
		if (iov_index <= 0)
		{
			return err;
		}

		// Send out iovs at a time.
		if ((err = do_iovs_send(out_iovs, iov_index)) != srs_success)
		{
			return srs_error_wrap(err, "send iovs");
		}

		return err;
#else
		// // try to send use the c0c3 header cache,
		// // if cache is consumed, try another loop.
		// for (int i = 0; i < nb_msgs; i++)
		// {
		// 	RtmpSharedPtrMessage* msg = msgs[i];

		// 	if (!msg)
		// 	{
		// 		continue;
		// 	}

		// 	// ignore empty message.
		// 	if (!msg->payload || msg->size <= 0)
		// 	{
		// 		continue;
		// 	}

		// 	// p set to current write position,
		// 	// it's ok when payload is nullptr and size is 0.
		// 	char* p    = msg->payload;
		// 	char* pend = msg->payload + msg->size;

		// 	// always write the header event payload is empty.
		// 	while (p < pend)
		// 	{
		// 		// for simple send, send each chunk one by one
		// 		iovec* iovs      = out_iovs;
		// 		char* c0c3_cache = out_c0c3_caches;
		// 		int nb_cache     = SRS_CONSTS_C0C3_HEADERS_MAX;

		// 		// always has header
		// 		int nbh = msg->chunk_header(c0c3_cache, nb_cache, p == msg->payload);
		// 		srs_assert(nbh > 0);

		// 		// header iov
		// 		iovs[0].iov_base = c0c3_cache;
		// 		iovs[0].iov_len  = nbh;

		// 		// payload iov
		// 		int payload_size = std::min(out_chunk_size, pend - p);
		// 		iovs[1].iov_base = p;
		// 		iovs[1].iov_len  = payload_size;

		// 		// consume sendout bytes.
		// 		p += payload_size;

		// 		if ((er = skt->writev(iovs, 2, nullptr)) != srs_success)
		// 		{
		// 			return srs_error_wrap(err, "writev");
		// 		}
		// 	}
		// }

		// return err;
#endif
	}

	srs_error_t RtmpTcpConnection::do_iovs_send(uv_buf_t* iovs, int size)
	{
		srs_error_t err = srs_success;
		if (b_showDebugLog)
		{
			MS_DEBUG_DEV_STD("do_iovs_send send size=%d", size);
		}
		size_t len = 0;

		std::vector<WriteData> datas;
		for (int i = 0; i < size; ++i)
		{
			uv_buf_t* iov = iovs + i;
			len += iov->len;
			datas.emplace_back((const uint8_t*)iov->base, (size_t)iov->len);
		}

		::TcpConnectionHandler::Write(datas, nullptr);

		if (b_showDebugLog)
		{
			MS_DEBUG_DEV_STD("do_iovs_send send len=%" PRIu64 "", len);
		}
		return err;
	}

	srs_error_t RtmpTcpConnection::decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket)
	{
		*ppacket = nullptr;

		srs_error_t err = srs_success;

		srs_assert(msg != nullptr);
		srs_assert(msg->payload != nullptr);
		srs_assert(msg->size > 0);

		Utils::RtmpBuffer stream(msg->payload, msg->size);

		// decode the packet.
		RtmpPacket* packet = nullptr;
		if ((err = do_decode_message(msg->header, &stream, &packet)) != srs_success)
		{
			FREEP(packet);
			return srs_error_wrap(err, "decode message");
		}

		// set to output ppacket only when success.
		*ppacket = packet;

		return err;
	}

	srs_error_t RtmpTcpConnection::do_decode_message(
	  RtmpMessageHeader& header,
	  Utils::RtmpBuffer* stream,
	  RtmpPacket** ppacket) // [dming] do_decode_message
	{
		srs_error_t err = srs_success;

		RtmpPacket* packet = nullptr;

		// decode specified packet type
		if (header.is_amf0_command() || header.is_amf3_command() || header.is_amf0_data() || header.is_amf3_data())
		{
			// skip 1bytes to decode the amf3 command.
			if (header.is_amf3_command() && stream->require(1))
			{
				stream->skip(1);
			}

			// amf0 command message.
			// need to read the command name.
			std::string command;
			if ((err = srs_amf0_read_string(stream, command)) != srs_success)
			{
				return srs_error_wrap(err, "decode command name");
			}

			// result/error packet
			if (command == RTMP_AMF0_COMMAND_RESULT || command == RTMP_AMF0_COMMAND_ERROR)
			{
				double transactionId = 0.0;
				if ((err = srs_amf0_read_number(stream, transactionId)) != srs_success)
				{
					return srs_error_wrap(err, "decode tid for %s", command.c_str());
				}

				// reset stream, for header read completed.
				stream->skip(-1 * stream->pos());
				if (header.is_amf3_command())
				{
					stream->skip(1);
				}

				// find the call name
				if (requests.find(transactionId) == requests.end())
				{
					return srs_error_new(
					  ERROR_RTMP_NO_REQUEST,
					  "find request for command=%s, tid=%.2f",
					  command.c_str(),
					  transactionId);
				}

				std::string request_name = requests[transactionId];
				if (request_name == RTMP_AMF0_COMMAND_CONNECT)
				{
					*ppacket = packet = new RtmpConnectAppResPacket();
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_CREATE_STREAM)
				{
					*ppacket = packet = new RtmpCreateStreamResPacket(0, 0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_RELEASE_STREAM)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_FC_PUBLISH)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_UNPUBLISH)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else
				{
					return srs_error_new(
					  ERROR_RTMP_NO_REQUEST, "request=%s, tid=%.2f", request_name.c_str(), transactionId);
				}
			}

			// reset to zero(amf3 to 1) to restart decode.
			stream->skip(-1 * stream->pos());
			if (header.is_amf3_command())
			{
				stream->skip(1);
			}

			// decode command object.
			if (command == RTMP_AMF0_COMMAND_CONNECT)
			{
				*ppacket = packet = new RtmpConnectAppPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_CREATE_STREAM)
			{
				*ppacket = packet = new RtmpCreateStreamPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PLAY)
			{
				*ppacket = packet = new RtmpPlayPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PAUSE)
			{
				*ppacket = packet = new RtmpPausePacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_RELEASE_STREAM)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_FC_PUBLISH)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PUBLISH)
			{
				*ppacket = packet = new RtmpPublishPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_UNPUBLISH)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == SRS_CONSTS_RTMP_SET_DATAFRAME)
			{
				*ppacket = packet = new RtmpOnMetaDataPacket();
				return packet->decode(stream);
			}
			else if (command == SRS_CONSTS_RTMP_ON_METADATA)
			{
				*ppacket = packet = new RtmpOnMetaDataPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_CLOSE_STREAM)
			{
				*ppacket = packet = new RtmpCloseStreamPacket();
				return packet->decode(stream);
			}
			else if (header.is_amf0_command() || header.is_amf3_command())
			{
				// as command is "getStreamLength"
				MS_DEBUG_DEV_STD(
				  "Unknow command, command is %s . just return RtmpCallPacket", command.c_str());
				*ppacket = packet = new RtmpCallPacket();
				return packet->decode(stream);
			}

			// default packet to drop message.
			*ppacket = packet = new RtmpPacket();
			return err;
		}
		else if (header.is_user_control_message())
		{
			*ppacket = packet = new RtmpUserControlPacket();
			return packet->decode(stream);
		}
		else if (header.is_window_ackledgement_size())
		{
			*ppacket = packet = new RtmpSetWindowAckSizePacket();
			return packet->decode(stream);
		}
		else if (header.is_ackledgement())
		{
			*ppacket = packet = new RtmpAcknowledgementPacket();
			return packet->decode(stream);
		}
		else if (header.is_set_chunk_size())
		{
			*ppacket = packet = new RtmpSetChunkSizePacket();
			return packet->decode(stream);
		}
		else
		{
			if (!header.is_set_peer_bandwidth() && !header.is_ackledgement())
			{
				MS_DEBUG_DEV_STD("drop unknown message, type=%d", header.message_type);
			}
		}

		return err;
	}

	srs_error_t RtmpTcpConnection::on_send_packet(RtmpMessageHeader* mh, RtmpPacket* packet)
	{
		srs_error_t err = srs_success;

		// ignore raw bytes oriented RTMP message.
		if (packet == nullptr)
		{
			return err;
		}

		switch (mh->message_type)
		{
			case RTMP_MSG_SetChunkSize:
			{
				RtmpSetChunkSizePacket* pkt = dynamic_cast<RtmpSetChunkSizePacket*>(packet);
				out_chunk_size              = pkt->chunk_size;
				break;
			}
			case RTMP_MSG_WindowAcknowledgementSize:
			{
				RtmpSetWindowAckSizePacket* pkt = dynamic_cast<RtmpSetWindowAckSizePacket*>(packet);
				out_ack_size.window             = (uint32_t)pkt->ackowledgement_window_size;
				break;
			}
			case RTMP_MSG_AMF0CommandMessage:
			case RTMP_MSG_AMF3CommandMessage:
			{
				if (true)
				{
					RtmpConnectAppPacket* pkt = dynamic_cast<RtmpConnectAppPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				if (true)
				{
					RtmpCreateStreamPacket* pkt = dynamic_cast<RtmpCreateStreamPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				if (true)
				{
					RtmpFMLEStartPacket* pkt = dynamic_cast<RtmpFMLEStartPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				break;
			}
			case RTMP_MSG_VideoMessage:
			case RTMP_MSG_AudioMessage:
				print_debug_info();
			default:
				break;
		}

		return err;
	}

	void RtmpTcpConnection::print_debug_info()
	{
		if (show_debug_info)
		{
			show_debug_info = false;
			MS_DEBUG_DEV_STD(
			  "protocol in.buffer=%d, in.ack=%d, out.ack=%d, in.chunk=%d, out.chunk=%d",
			  in_buffer_length,
			  in_ack_size.window,
			  out_ack_size.window,
			  in_chunk_size,
			  out_chunk_size);
		}
	}

	srs_error_t RtmpTcpConnection::response_ping_message(int32_t timestamp)
	{
		srs_error_t err = srs_success;

		MS_DEBUG_DEV_STD("get a ping request, response it. timestamp=%d", timestamp);

		RtmpUserControlPacket* pkt = new RtmpUserControlPacket();

		pkt->event_type = SrcPCUCPingResponse;
		pkt->event_data = timestamp;

		// // cache the message and use flush to send.
		// if (!auto_response_when_recv)
		// {
		// 	manual_response_queue.push_back(pkt);
		// 	return err;
		// }

		// use underlayer api to send, donot flush again.
		if ((err = send_and_free_packet(pkt, 0)) != srs_success)
		{
			return srs_error_wrap(err, "ping response");
		}

		return err;
	}

	srs_error_t RtmpTcpConnection::response_acknowledgement_message()
	{
		srs_error_t err = srs_success;

		if (in_ack_size.window <= 0)
		{
			return err;
		}

		// ignore when delta bytes not exceed half of window(ack size).
		uint32_t delta = (uint32_t)(GetRecvBytes() - in_ack_size.nb_recv_bytes);
		if (delta < in_ack_size.window / 2)
		{
			return err;
		}
		in_ack_size.nb_recv_bytes = GetRecvBytes();

		// when the sequence number overflow, reset it.
		uint32_t sequence_number = in_ack_size.sequence_number + delta;
		if (sequence_number > 0xf0000000)
		{
			sequence_number = delta;
		}
		in_ack_size.sequence_number = sequence_number;

		RtmpAcknowledgementPacket* pkt = new RtmpAcknowledgementPacket();
		pkt->sequence_number           = sequence_number;

		// // cache the message and use flush to send.
		// if (!auto_response_when_recv)
		// {
		// 	manual_response_queue.push_back(pkt);
		// 	return err;
		// }

		// use underlayer api to send, donot flush again.
		if ((err = do_send_and_free_packet(pkt, 0)) != srs_success)
		{
			return srs_error_wrap(err, "send ack");
		}

		return err;
	}
} // namespace RTMP
