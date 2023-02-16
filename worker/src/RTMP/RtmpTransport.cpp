#define MS_CLASS "RTMP::RtmpTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTransport.hpp"
#include "Logger.hpp"

namespace RTMP
{
	RtmpTransport::RtmpTransport() : connection(new RTMP::RtmpTcpConnection(this, 65535))
	{
		MS_TRACE();
	}

	RtmpTransport::~RtmpTransport()
	{
		MS_TRACE();
	}

	void RtmpTransport::OnTcpConnectionPacketReceived(
	  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg)
	{
		MS_TRACE();
		RTC::TransportTuple tuple(connection);
		OnRecvMessage(&tuple, msg);
	}

	void RtmpTransport::OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		MS_TRACE();
		MS_DEBUG_DEV("msg type is %d", msg->header.message_type);
	}

	RtmpTcpConnection* RtmpTransport::GetConnection()
	{
		return connection;
	}
} // namespace RTMP