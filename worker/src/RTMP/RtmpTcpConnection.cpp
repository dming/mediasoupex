#define MS_CLASS "RTMP::RtmpTcpConnection"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpConnection.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpCommon.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "Utils.hpp"
#include <cstring> // std::memmove(), std::memcpy()
#include <mutex>

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

static std::mutex globalReadMutex;

namespace RTMP
{

	RtmpTcpConnection::RtmpTcpConnection(Listener* listener, size_t bufferSize)
	  : ::TcpConnectionHandler::TcpConnectionHandler(bufferSize), listener(listener),
	    hsBytes(new RtmpHandshakeBytes()), in_chunk_size(SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE),
	    b_showDebugLog(true)
	{
		MS_TRACE();

		in_chunk_size  = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
		out_chunk_size = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;

		nb_out_iovs = 8 * SRS_CONSTS_IOVS_MAX;
		out_iovs    = (uv_buf_t*)malloc(sizeof(uv_buf_t) * nb_out_iovs);

		warned_c0c3_cache_dry = false;

		chunkStreamCache = NULL;
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
		MS_DEBUG_DEV("~RtmpTcpConnection Free ip:%s, port:%" PRIu16, GetPeerIp().c_str(), GetPeerPort());
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

	/**
	 * 每次收到数据都会调用UserOnTcpConnectionRead
	 * handshake 数据也应该在这里实现，传入this.buffer* 及 recvBytes*即可在外部读取handshake字节。
	 */
	void RtmpTcpConnection::UserOnTcpConnectionRead()
	{
		static uint32_t times = 0;
		MS_TRACE();

		if (b_showDebugLog)
		{
			MS_DEBUG_DEV(
			  "data received [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "] unRead:%" PRIu64
			  ", times:%" PRIu32 ".",
			  GetLocalIp().c_str(),
			  GetLocalPort(),
			  GetPeerIp().c_str(),
			  GetPeerPort(),
			  (size_t)(this->bufferDataLen - this->frameStart),
			  times++);
		}

		// std::lock_guard<std::mutex> lock(globalReadMutex);
		/**
		 * if !handshake then run Handshake.class until handshake done.
		 */
		if (hsBytes->done)
		{
			/**
			 * read rtmp tcp pecket;
			 */
			int nnn = 1000;
			while (true) // true
			{
				nnn--;
				// [dming] TODO: 一直读取rtmp packet 直到数据不足
				if (nnn < 0 || IsClosed())
					return;

				/**
				 * 需要先解析出header，再根据header去解析出body
				 */
				size_t dataLen = this->bufferDataLen - this->frameStart;
				if (dataLen == 0)
					return;
				// size_t packetLen;

				RtmpCommonMessage* msg = nullptr;
				if (RecvInterlacedMessage(&msg) != 0)
				{
					FREEPA(msg);
					if (b_showDebugLog)
					{
						MS_DEBUG_DEV("recv interlaced message");
					}
					return;
				}

				if (!msg)
				{
					continue;
				}

				if (msg->size <= 0 || msg->header.payload_length <= 0)
				{
					if (b_showDebugLog)
					{
						MS_DEBUG_DEV(
						  "ignore empty message(type=%d, size=%d, time=%" PRId64 ", sid=%d).",
						  msg->header.message_type,
						  msg->header.payload_length,
						  msg->header.timestamp,
						  msg->header.stream_id);
					}
					FREEPA(msg);
					continue;
				}

				// 获得的msg直接抛给上层
				this->listener->OnTcpConnectionPacketReceived(this, msg);
			}
		}
		else
		{
			RtmpHandshake handshake;
			handshake.HandshakeWithClient(hsBytes, this);
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
				MS_ERROR("not enought data for size %" PRIu64 ", only have %" PRIu64, dataSize, dataLen);
			}
			return -1;
		}

		const uint8_t* packet = this->buffer + this->frameStart;
		std::memcpy(data, packet, dataSize);

		// If there is no more space available in the buffer and that is because
		// the latest parsed frame filled it, then empty the full buffer.
		if (this->frameStart + dataSize == this->bufferSize)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV("no more space in the buffer, emptying the buffer data");
			}

			this->frameStart    = 0;
			this->bufferDataLen = 0;
		}
		// If there is still space in the buffer, set the beginning of the next
		// frame to the next position after the parsed frame.
		else
		{
			this->frameStart += dataSize;
		}

		return 0;
	}

	void RtmpTcpConnection::Send(
	  const uint8_t* data, size_t len, ::TcpConnectionHandler::onSendCallback* cb)
	{
		MS_TRACE();
		::TcpConnectionHandler::Write(data, len, cb);
	}

	int RtmpTcpConnection::RecvInterlacedMessage(RtmpCommonMessage** pmsg)
	{
		MS_TRACE();
		int err = 0;
		// chunk stream basic header.
		char fmt  = 0;
		int cid   = 0;
		int bhLen = 0;
		if ((err = ReadBasicHeader(fmt, cid, bhLen)) != 0)
		{
			MS_DEBUG_DEV("read basic header wrong");
			return err;
		}

		// the cid must not negative.
		MS_ASSERT(cid >= 0, "the cid must not negative.");
		if (b_showDebugLog)
		{
			MS_DEBUG_DEV("cid is %d", cid);
		}

		// get the cached chunk stream.
		RtmpChunkStream* chunk = NULL;

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

		if ((err = ReadMessageHeader(chunk, fmt, bhLen)) != 0)
		{
			if (b_showDebugLog)
			{
				MS_ERROR("read message header fail");
			}
			return err;
		}

		// read msg payload from chunk stream.
		RtmpCommonMessage* msg = nullptr;
		if ((err = ReadMessagePayload(chunk, &msg)) != 0) // [dming] third: read full body
		{
			if (b_showDebugLog)
			{
				MS_ERROR("read message payload");
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
	int RtmpTcpConnection::ReadBasicHeader(char& fmt, int& cid, int& bhLen)
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
				MS_DEBUG_DEV("basic header requires at least 1 bytes");
			}
			return -1;
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
					MS_DEBUG_DEV("basic header requires 2 bytes");
				}
				return -1;
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
					MS_DEBUG_DEV("basic header requires 3 bytes");
				}
				return -1;
			}
			cid = 64;
			cid += (uint8_t)*p++;
			cid += ((uint8_t)*p++) * 256;
			bhLen = 3;
		}

		return 0;
	}

	int RtmpTcpConnection::ReadMessageHeader(RtmpChunkStream* chunk, char fmt, int bhLen)
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
		 * (when first packet, the chunk->msg is NULL).
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
				MS_ERROR("fresh chunk expect fmt=0, actual=%d, cid=%d", fmt, chunk->cid);
				return -1;
			}
		}

		// when exists cache msg, means got an partial message,
		// the fmt must not be type0 which means new message.
		if (chunk->msg && fmt == RTMP_FMT_TYPE0)
		{
			// return srs_error_new(ERROR_RTMP_CHUNK_START, "for existed chunk, fmt should not be 0");
			MS_ERROR("for existed chunk, fmt should not be 0");
			return -1;
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
				MS_ERROR("cannot read %" PRId32 " bytes message header", bhLen + mhSize);
			}
			return -1;
		}

		MessageHeader header;
		memset(&header, 0, sizeof(MessageHeader));
		memcpy(&header, this->buffer + this->frameStart + bhLen, mhSize);

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
			int32_t timestamp_delta = Utils::Byte::ReadUint24BE((char*)header.timestampDelta); // 改成大端序

			bool extenedTimestamp = (timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);
			if (extenedTimestamp)
			{
				mhSize += 4;
				if (dataLen < bhLen + mhSize)
				{
					if (b_showDebugLog)
					{
						MS_ERROR("cannot read %" PRId32 " bytes message header", bhLen + mhSize);
					}
					goto NOT_ENOUGH_DATA;
				}
			}
			int32_t payloadLen = chunk->header.payload_length;
			if (fmt <= RTMP_FMT_TYPE1)
			{
				payloadLen = Utils::Byte::ReadUint24BE((char*)header.payloadLength);
			}
			int payloadSize = payloadLen - chunk->msg->size;
			payloadSize     = std::min(payloadSize, in_chunk_size);
			if (payloadSize > 0 && dataLen < bhLen + mhSize + payloadSize)
			{
				if (b_showDebugLog)
				{
					MS_ERROR(
					  "fmt=%d, cannot read %d bytes message header, and %d bytes payload size, only has %" PRIu64,
					  fmt,
					  bhLen + mhSize,
					  payloadLen - chunk->msg->size,
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
			chunk->extended_timestamp = extenedTimestamp;
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
				int32_t payload_length = payloadLen;

				// for a message, if msg exists in cache, the size must not changed.
				// always use the actual msg size to compare, for the cache payload length can changed,
				// for the fmt type1(stream_id not changed), user can change the payload
				// length(it's not allowed in the continue chunks).
				if (!is_first_chunk_of_msg && chunk->header.payload_length != payload_length)
				{
					MS_ERROR(
					  "msg in chunk cache, size=%d cannot change to %d",
					  chunk->header.payload_length,
					  payload_length);
					return -1;
				}

				chunk->header.payload_length = payload_length;
				chunk->header.message_type   = header.messageType;

				if (fmt == RTMP_FMT_TYPE0)
				{
					chunk->header.stream_id = Utils::Byte::ReadUint24LE((char*)header.streamId);
				}
			}
		}
		else
		{
			int32_t payloadLen = chunk->header.payload_length;
			int payloadSize    = payloadLen - chunk->msg->size;
			payloadSize        = std::min(payloadSize, in_chunk_size);
			if (payloadSize > 0 && dataLen < (bhLen + mhSize + payloadSize))
			{
				if (b_showDebugLog)
				{
					MS_ERROR(
					  "fmt=%d, cannot read %d bytes message header, and %d bytes payload size, only %" PRIu64,
					  fmt,
					  bhLen + mhSize,
					  payloadLen - chunk->msg->size,
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

		return 0;

	NOT_ENOUGH_DATA:
		if (chunk->msg && chunk->msg->size == 0)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV("delete chunk->msg");
			}
			delete chunk->msg;
			chunk->msg = nullptr;
		}

		// Check if the buffer is full.
		if (this->bufferDataLen == this->bufferSize)
		{
			// First case: the incomplete frame does not begin at position 0 of
			// the buffer, so move the frame to the position 0.
			if (this->frameStart != 0)
			{
				if (b_showDebugLog)
				{
					MS_DEBUG_DEV(
					  "no more space in the buffer, moving parsed bytes to the beginning of "
					  "the buffer and wait for more data");
				}

				std::memmove(
				  this->buffer, this->buffer + this->frameStart, this->bufferSize - this->frameStart);
				this->bufferDataLen = this->bufferSize - this->frameStart;
				this->frameStart    = 0;
			}
			// Second case: the incomplete frame begins at position 0 of the buffer.
			// The frame is too big.
			else
			{
				MS_WARN_DEV(
				  "no more space in the buffer for the unfinished frame being parsed, closing the "
				  "connection");

				ErrorReceiving();

				// And exit fast since we are supposed to be deallocated.
				return -1;
			}
		}
		// The buffer is not full.
		else
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV("frame not finished yet, waiting for more data");
			}
		}
		return -1;
	}

	int RtmpTcpConnection::ReadMessagePayload(RtmpChunkStream* chunk, RtmpCommonMessage** pmsg)
	{
		MS_TRACE();

		int err = 0;
		// empty message
		if (chunk->header.payload_length <= 0)
		{
			if (b_showDebugLog)
			{
				MS_DEBUG_DEV(
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
				MS_DEBUG_DEV("create payload for RTMP message. size=%d", chunk->header.payload_length);
			}
		}
		memcpy(chunk->msg->payload + chunk->msg->size, this->buffer + this->frameStart, payload_size);
		chunk->msg->size += payload_size;
		if (b_showDebugLog)
		{
			MS_DEBUG_DEV(
			  "chunk msg size is :%d, and payload length is %d",
			  chunk->msg->size,
			  chunk->header.payload_length);
		}

		if (this->frameStart + payload_size == this->bufferSize)
		{
			MS_DEBUG_DEV("no more space in the buffer, emptying the buffer data");
			this->frameStart    = 0;
			this->bufferDataLen = 0;
		}
		else
		{
			this->frameStart += payload_size;
		}

		// got entire RTMP message?
		if (chunk->header.payload_length == chunk->msg->size)
		{
			*pmsg      = chunk->msg;
			chunk->msg = NULL;
			return err;
		}

		return err;
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
		// std::unique_ptr<RtmpPacket> _auto_free_packet(packet);
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

		// if ((err = on_send_packet(&msg->header, packet)) != srs_success) //[dming]
		// TODO:先忽略，后面再补
		// {
		// 	return srs_error_wrap(err, "on send packet");
		// }

		return err;
	}

	srs_error_t RtmpTcpConnection::send_and_free_message(RtmpSharedPtrMessage* msg, int stream_id)
	{
		return send_and_free_messages(&msg, 1, stream_id);
	}

	srs_error_t RtmpTcpConnection::send_and_free_messages(
	  RtmpSharedPtrMessage** msgs, int nb_msgs, int stream_id)
	{
		// always not NULL msg.
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
			// it's ok when payload is NULL and size is 0.
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

				MS_DEBUG_DEV("add header size=%d, payload size=%d", nbh, payload_size);

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
			// it's ok when payload is NULL and size is 0.
			char* p    = msg->payload;
			char* pend = msg->payload + msg->size;

			// always write the header event payload is empty.
			while (p < pend)
			{
				// for simple send, send each chunk one by one
				iovec* iovs      = out_iovs;
				char* c0c3_cache = out_c0c3_caches;
				int nb_cache     = SRS_CONSTS_C0C3_HEADERS_MAX;

				// always has header
				int nbh = msg->chunk_header(c0c3_cache, nb_cache, p == msg->payload);
				srs_assert(nbh > 0);

				// header iov
				iovs[0].iov_base = c0c3_cache;
				iovs[0].iov_len  = nbh;

				// payload iov
				int payload_size = std::min(out_chunk_size, pend - p);
				iovs[1].iov_base = p;
				iovs[1].iov_len  = payload_size;

				// consume sendout bytes.
				p += payload_size;

				if ((er = skt->writev(iovs, 2, NULL)) != srs_success)
				{
					return srs_error_wrap(err, "writev");
				}
			}
		}

		return err;
#endif
	}

	srs_error_t RtmpTcpConnection::do_iovs_send(uv_buf_t* iovs, int size)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV("do_iovs_send send size=%d", size);
		size_t len = 0;
		for (int i = 0; i < size; ++i)
		{
			uv_buf_t* iov = iovs + i;
			len += iov->len;
		}
		char* data = new char[len];
		len        = 0;
		for (int i = 0; i < size; ++i)
		{
			uv_buf_t* iov = iovs + i;
			memcpy(data + len, iov->base, iov->len);
			len += iov->len;
			// data += iov->len;
		}

		Send((const uint8_t*)data, len, nullptr);
		MS_DEBUG_DEV("do_iovs_send send len=%" PRIu64 "", len);
		return err;

		// 		// the limits of writev iovs.
		// #ifndef _WIN32
		// 		// for linux, generally it's 1024.
		// 		static int limits = (int)sysconf(_SC_IOV_MAX);
		// #else
		// 		static int limits = 1024;
		// #endif

		// 		// send in a time.
		// 		if (size <= limits)
		// 		{
		// 			Send((const uint8_t*)iovs, size, nullptr);
		// 			return err;
		// 		}

		// 		// send in multiple times.
		// 		int cur_iov    = 0;
		// 		ssize_t nwrite = 0;
		// 		while (cur_iov < size)
		// 		{
		// 			int cur_count = std::min(limits, size - cur_iov);
		// 			Send((const uint8_t*)iovs + cur_iov, cur_count, nullptr);
		// 			cur_iov += cur_count;
		// 		}

		// 		return err;
	}
} // namespace RTMP
