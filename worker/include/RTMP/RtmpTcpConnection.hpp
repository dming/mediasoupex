#ifndef MS_RTMP_TCP_CONNECTION_HPP
#define MS_RTMP_TCP_CONNECTION_HPP

#include "common.hpp"
#include "RTMP/RtmpHandshake.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "handles/TcpConnectionHandler.hpp"
#include <map>

namespace RTMP
{
	class RtmpTcpConnection : public ::TcpConnectionHandler
	{
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
		int RecvInterlacedMessage(RtmpCommonMessage** pmsg);
		int ReadBasicHeader(char& fmt, int& cid, int& bhLen);
		int ReadMessageHeader(RtmpChunkStream* chunk, char fmt, int bhLen);
		int ReadMessagePayload(RtmpChunkStream* chunk, RtmpCommonMessage** pmsg);

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
	};
} // namespace RTMP

#endif
