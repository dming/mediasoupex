#define MS_CLASS "RTMP::RtmpTcpConnection"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTcpConnection.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHandshake.hpp"
#include "Utils.hpp"
#include <cstring> // std::memmove(), std::memcpy()

namespace RTMP
{
	RtmpTcpConnection::RtmpTcpConnection(Listener* listener, size_t bufferSize)
	  : ::TcpConnectionHandler::TcpConnectionHandler(bufferSize), listener(listener),
	    hsBytes(new RtmpHandshakeBytes())
	{
		MS_TRACE();
	}

	RtmpTcpConnection::~RtmpTcpConnection()
	{
		MS_TRACE();
		FREEP(hsBytes);
	}

	/**
	 * 每次收到数据都会调用UserOnTcpConnectionRead
	 * handshake 数据也应该在这里实现，传入this.buffer* 及 recvBytes*即可在外部读取handshake字节。
	 */
	void RtmpTcpConnection::UserOnTcpConnectionRead()
	{
		MS_TRACE();

		MS_DEBUG_DEV(
		  "data received [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "]",
		  GetLocalIp().c_str(),
		  GetLocalPort(),
		  GetPeerIp().c_str(),
		  GetPeerPort());

		/**
		 * if !handshake then run Handshake.class until handshake done.
		 */
		if (hsBytes->done)
		{
			/**
			 * read rtmp tcp pecket;
			 */
			while (false) // true
			{
				// [dming] TODO: 一直读取rtmp packet 直到数据不足
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
			MS_DEBUG_DEV("not enought data for size %" PRIu32 ", only have %" PRIu32, dataSize, dataLen);
			return -1;
		}

		const uint8_t* packet = this->buffer + this->frameStart;
		std::memcpy(data, packet, dataSize);

		// If there is no more space available in the buffer and that is because
		// the latest parsed frame filled it, then empty the full buffer.
		if (this->frameStart + dataSize == this->bufferSize)
		{
			MS_DEBUG_DEV("no more space in the buffer, emptying the buffer data");

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
} // namespace RTMP
