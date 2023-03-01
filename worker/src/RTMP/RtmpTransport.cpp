#define MS_CLASS "RTMP::RtmpTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTransport.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"

namespace RTMP
{
	RtmpTransport::RtmpTransport()
	  : connection(new RTMP::RtmpTcpConnection(this, 65535)), protocol(new RtmpProtocol()),
	    info(new RtmpClientInfo())
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

	srs_error_t RtmpTransport::OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		MS_TRACE();
		MS_DEBUG_DEV("msg type is %d", msg->header.message_type);
		srs_error_t err = srs_success;

		RTMP::RtmpPacket* packet;
		if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpConnectAppPacket* m_packet = dynamic_cast<RtmpConnectAppPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpConnectAppPacket ");
				return HandleRtmpConnectAppPacket(m_packet);
			}
			else if (RtmpCreateStreamPacket* m_packet = dynamic_cast<RtmpCreateStreamPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpCreateStreamPacket ");
			}
			else if (RtmpPlayPacket* m_packet = dynamic_cast<RtmpPlayPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpPlayPacket ");
			}
			else if (RtmpPausePacket* m_packet = dynamic_cast<RtmpPausePacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpPausePacket ");
			}
			else if (RtmpFMLEStartPacket* m_packet = dynamic_cast<RtmpFMLEStartPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpFMLEStartPacket ");
			}
			else if (RtmpPublishPacket* m_packet = dynamic_cast<RtmpPublishPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpPublishPacket ");
			}
			else if (RtmpCloseStreamPacket* m_packet = dynamic_cast<RtmpCloseStreamPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpCloseStreamPacket ");
			}
			else if (RtmpCallPacket* m_packet = dynamic_cast<RtmpCallPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpCallPacket ");
			}
		}
		else if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
		}
		else if (msg->header.is_user_control_message())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpUserControlPacket ");
		}
		else if (msg->header.is_window_ackledgement_size())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpSetWindowAckSizePacket ");
		}
		else if (msg->header.is_ackledgement())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpAcknowledgementPacket ");
		}
		else if (msg->header.is_set_chunk_size())
		{
			if ((err = protocol->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpSetChunkSizePacket ");
		}
		return err;
	}

	srs_error_t RtmpTransport::HandleRtmpConnectAppPacket(RtmpConnectAppPacket* packet)
	{
		MS_TRACE();
		SrsAmf0Any* prop = NULL;

		RtmpRequest* req = info->req;

		if ((prop = packet->command_object->ensure_property_string("tcUrl")) == NULL)
		{
			return srs_error_new(ERROR_RTMP_REQ_CONNECT, "invalid request without tcUrl");
		}
		req->tcUrl = prop->to_str(); // [dming] 客户端告诉服务器请求的url信息

		if ((prop = packet->command_object->ensure_property_string("pageUrl")) != NULL)
		{
			req->pageUrl = prop->to_str();
		}

		if ((prop = packet->command_object->ensure_property_string("swfUrl")) != NULL)
		{
			req->swfUrl = prop->to_str();
		}

		if ((prop = packet->command_object->ensure_property_number("objectEncoding")) != NULL)
		{
			req->objectEncoding = prop->to_number();
		}

		// tcUrl=rtmp://192.168.167.88:19350/aaa
		MS_DEBUG_DEV(
		  "HandleRtmpConnectAppPacket tcUrl=%s, pageUrl=%s, swfUrl=%s, objectEncoding=%lf",
		  req->tcUrl.c_str(),
		  req->pageUrl.c_str(),
		  req->swfUrl.c_str(),
		  req->objectEncoding);

		if (packet->args)
		{
			FREEP(req->args);
			req->args = packet->args->copy()->to_object();
		}

		srs_discovery_tc_url(
		  req->tcUrl, req->schema, req->host, req->vhost, req->app, req->stream, req->port, req->param);
		req->strip();
		MS_DEBUG_DEV(
		  "[2] HandleRtmpConnectAppPacket schema=%s, stream=%s, app=%s",
		  req->schema.c_str(),
		  req->stream.c_str(),
		  req->app.c_str());
		return srs_success;
	}
} // namespace RTMP