#ifndef MS_RTMP_TCP_CONNECTION_HPP
#define MS_RTMP_TCP_CONNECTION_HPP

#include "common.hpp"
#include "RTMP/RtmpHandshake.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"
#include "handles/TcpConnectionHandler.hpp"
#include <uv.h>
#include <map>

namespace RTMP
{
	class RtmpTcpConnection : public ::TcpConnectionHandler
	{
	private:
		class AckWindowSize
		{
		public:
			uint32_t window;
			// number of received bytes.
			int64_t nb_recv_bytes;
			// previous responsed sequence number.
			uint32_t sequence_number;

			AckWindowSize();
		};

	public:
		class Listener
		{
		public:
			virtual ~Listener() = default;

		public:
			virtual void OnTcpConnectionPacketReceived(
			  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg) = 0;
		};

	public:
		RtmpTcpConnection(Listener* listener, size_t bufferSize);
		~RtmpTcpConnection() override;

	public:
		void Send(const uint8_t* data, size_t len, ::TcpConnectionHandler::onSendCallback* cb) override;
		int ReadFully(char* buf, size_t size);

		/* Pure virtual methods inherited from ::TcpConnectionHandler. */
	public:
		void UserOnTcpConnectionRead() override;

	private:
		virtual srs_error_t RecvInterlacedMessage(RtmpCommonMessage** pmsg);
		virtual srs_error_t ReadBasicHeader(char& fmt, int& cid, int& bhLen);
		virtual srs_error_t ReadMessageHeader(RtmpChunkStream* chunk, char fmt, int bhLen);
		virtual srs_error_t ReadMessagePayload(RtmpChunkStream* chunk, RtmpCommonMessage** pmsg);
		virtual void AfterReadBuffer(std::string log);
		// Recv complete message
		virtual void OnRecvMessage(RtmpCommonMessage* msg);
		// Auto response the ping message.
		virtual srs_error_t response_ping_message(int32_t timestamp);
		// Auto response the ack message.
		virtual srs_error_t response_acknowledgement_message();

	public:
		// Send the RTMP message and always free it.
		// user must never free or use the msg after this method,
		// For it will always free the msg.
		// @param msg, the msg to send out, never be nullptr.
		// @param stream_id, the stream id of packet to send over, 0 for control message.
		virtual srs_error_t send_and_free_message(RtmpSharedPtrMessage* msg, int stream_id);
		// Send the RTMP message and always free it.
		// user must never free or use the msg after this method,
		// For it will always free the msg.
		// @param msgs, the msgs to send out, never be nullptr.
		// @param nb_msgs, the size of msgs to send out.
		// @param stream_id, the stream id of packet to send over, 0 for control message.
		virtual srs_error_t send_and_free_messages(RtmpSharedPtrMessage** msgs, int nb_msgs, int stream_id);
		// Send the RTMP packet and always free it.
		// user must never free or use the packet after this method,
		// For it will always free the packet.
		// @param packet, the packet to send out, never be nullptr.
		// @param stream_id, the stream id of packet to send over, 0 for control message.
		virtual srs_error_t send_and_free_packet(RtmpPacket* packet, int stream_id);

	private:
		// Send out the messages, donot free it,
		// The caller must free the param msgs.
		virtual srs_error_t do_send_messages(RtmpSharedPtrMessage** msgs, int nb_msgs);
		// Send iovs. send multiple times if exceed limits.
		srs_error_t do_iovs_send(uv_buf_t* iovs, int size);
		// The underlayer api for send and free packet.
		virtual srs_error_t do_send_and_free_packet(RtmpPacket* packet, int stream_id);

		// >>> For peer in/out
	private:
		// The requests sent out, used to build the response.
		// key: transactionId
		// value: the request command name
		std::map<double, std::string> requests;

		// >>> For peer in
	private:
		// Passed by argument.
		Listener* listener{ nullptr };
		// Others.
		size_t frameStart{ 0u }; // Where the latest frame starts.
		RtmpHandshakeBytes* hsBytes;

		// The chunk stream to decode RTMP messages.
		std::map<int, RtmpChunkStream*> chunkStreams;
		// Cache some frequently used chunk header.
		// chunkStreamCache, the chunk stream cache.
		RtmpChunkStream** chunkStreamCache;
		// The input chunk size, default to 128, set by peer packet.
		int32_t in_chunk_size;
		// The input ack window, to response acknowledge to peer,
		// For example, to respose the encoder, for server got lots of packets.
		AckWindowSize in_ack_size;
		// The output ack window, to require peer to response the ack.
		AckWindowSize out_ack_size;
		// The buffer length set by peer.
		int32_t in_buffer_length;
		// Whether print the protocol level debug info.
		// Generally we print the debug info when got or send first A/V packet.
		bool show_debug_info;

		// >>> For peer out
	private:
		// Cache for multiple messages send,
		// initialize to iovec[SRS_CONSTS_IOVS_MAX] and realloc when consumed,
		// it's ok to realloc the iovs cache, for all ptr is ok.
		uv_buf_t* out_iovs;
		int nb_out_iovs;
		// The output header cache.
		// used for type0, 11bytes(or 15bytes with extended timestamp) header.
		// or for type3, 1bytes(or 5bytes with extended timestamp) header.
		// The c0c3 caches must use unit SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE bytes.
		//
		// @remark, the c0c3 cache cannot be realloc.
		// To allocate it in heap to make VS2015 happy.
		char* out_c0c3_caches;
		// Whether warned user to increase the c0c3 header cache.
		bool warned_c0c3_cache_dry;
		// The output chunk size, default to 128, set by config.
		int32_t out_chunk_size;

	public:
		// before video/audio stream start, show dev debug log;
		bool b_showDebugLog;

	public:
		// Decode bytes oriented RTMP message to RTMP packet,
		// @param ppacket, output decoded packet,
		//       always nullptr if error, never nullptr if success.
		// @return error when unknown packet, error when decode failed.
		virtual srs_error_t decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket);
		// When message sentout, update the context.
		virtual srs_error_t on_send_packet(RtmpMessageHeader* mh, RtmpPacket* packet);

	private:
		// The imp for decode_message
		virtual srs_error_t do_decode_message(
		  RtmpMessageHeader& header, Utils::RtmpBuffer* stream, RtmpPacket** ppacket);

		void print_debug_info();
	};
} // namespace RTMP

#endif
