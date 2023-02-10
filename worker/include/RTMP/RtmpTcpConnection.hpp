#ifndef MS_RTMP_TCP_CONNECTION_HPP
#define MS_RTMP_TCP_CONNECTION_HPP

#include "common.hpp"
#include "RTMP/RtmpHandshake.hpp"
#include "handles/TcpConnectionHandler.hpp"

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
			  RTMP::RtmpTcpConnection* connection, const uint8_t* data, size_t len) = 0;
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
		// Passed by argument.
		Listener* listener{ nullptr };
		// Others.
		size_t frameStart{ 0u }; // Where the latest frame starts.
		RtmpHandshakeBytes* hsBytes;
	};
} // namespace RTMP

#endif
