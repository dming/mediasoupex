#define MS_CLASS "RTMP::RtmpTcpConnection"

#include "RTMP/RtmpTcpConnection.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstring> // std::memmove(), std::memcpy()

namespace RTMP
{
    RtmpTcpConnection::RtmpTcpConnection(Listener* listener, size_t bufferSize)
      : ::TcpConnectionHandler::TcpConnectionHandler(bufferSize), listener(listener)
	{
		MS_TRACE();
	}


    RtmpTcpConnection::~RtmpTcpConnection()
    {
        MS_TRACE();
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
         * read rtmp tcp pecket;
        */
        /**
         * if !handshake then run Handshake.class until handshake done.
        */
    }
} // namespace RTMP
