#define MS_CLASS "RTMP::RtmpTransport"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpTransport.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpRouter.hpp"
#include "RTMP/RtmpServer.hpp"

// FMLE
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH "onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH "onFCUnpublish"

namespace RTMP
{
	RtmpTransport::RtmpTransport(RtmpServer* rtmpServer)
	  : rtmpServer_(rtmpServer), connection_(new RTMP::RtmpTcpConnection(this, 65535)),
	    protocol_(new RtmpProtocol()), info_(new RtmpClientInfo())
	{
		MS_TRACE();
	}

	RtmpTransport::~RtmpTransport()
	{
		MS_TRACE();

		// NOTE: connection will be free in TcpServerHandler::OnTcpConnectionClosed
		FREEP(info_);
		FREEP(protocol_);
		connection_ = nullptr;
		rtmpServer_ = nullptr;
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
		static size_t VAMsgCount = 0;
		if (msg->header.message_type == RTMP_MSG_VideoMessage || msg->header.message_type == RTMP_MSG_AudioMessage)
		{
			if (VAMsgCount % 300 == 0)
			{
				MS_DEBUG_DEV("msg type is %d", msg->header.message_type);
			}
			VAMsgCount++;
		}
		else
		{
			MS_DEBUG_DEV("msg type is %d", msg->header.message_type);
		}

		srs_error_t err = srs_success;

		RTMP::RtmpPacket* packet;
		if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
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
				return HandleRtmpCreateStreamPacket(m_packet);
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
				return HandleRtmpFMLEStartPacket(m_packet);
			}
			else if (RtmpPublishPacket* m_packet = dynamic_cast<RtmpPublishPacket*>(packet))
			{
				MS_DEBUG_DEV("OnRecvMessage RtmpPublishPacket ");
				return HandleRtmpPublishPacket(m_packet);
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
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
		}
		else if (msg->header.is_user_control_message())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpUserControlPacket ");
		}
		else if (msg->header.is_window_ackledgement_size())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpSetWindowAckSizePacket ");
		}
		else if (msg->header.is_ackledgement())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			MS_DEBUG_DEV("OnRecvMessage RtmpAcknowledgementPacket ");
		}
		else if (msg->header.is_set_chunk_size())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
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
		srs_error_t err = srs_success;

		RtmpAmf0Any* prop = NULL;

		RtmpRequest* req = info_->req;

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
		  "[2] HandleRtmpConnectAppPacket schema=%s,  app=%s,stream=%s",
		  req->schema.c_str(),
		  req->app.c_str(),
		  req->stream.c_str());

		response_connect_app(req, connection_->GetLocalIp().c_str());

		return srs_success;
	}

	srs_error_t RtmpTransport::response_connect_app(RtmpRequest* req, const char* server_ip)
	{
		srs_error_t err = srs_success;

		RtmpConnectAppResPacket* pkt = new RtmpConnectAppResPacket();

		// @remark For windows, there must be a space between const string and macro.
		pkt->props->set("fmsVer", RtmpAmf0Any::str("FMS/" RTMP_SIG_FMS_VER));
		pkt->props->set("capabilities", RtmpAmf0Any::number(127));
		pkt->props->set("mode", RtmpAmf0Any::number(1));

		pkt->info->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
		pkt->info->set(StatusCode, RtmpAmf0Any::str(StatusCodeConnectSuccess));
		pkt->info->set(StatusDescription, RtmpAmf0Any::str("Connection succeeded"));
		pkt->info->set("objectEncoding", RtmpAmf0Any::number(req->objectEncoding));
		RtmpAmf0EcmaArray* data = RtmpAmf0Any::ecma_array();
		pkt->info->set("data", data);

		data->set("version", RtmpAmf0Any::str(RTMP_SIG_FMS_VER));
		data->set("srs_sig", RtmpAmf0Any::str(RTMP_SIG_SRS_KEY));
		data->set("srs_server", RtmpAmf0Any::str(RTMP_SIG_SRS_SERVER));
		data->set("srs_license", RtmpAmf0Any::str(RTMP_SIG_SRS_LICENSE));
		data->set("srs_url", RtmpAmf0Any::str(RTMP_SIG_SRS_URL));
		data->set("srs_version", RtmpAmf0Any::str(RTMP_SIG_SRS_VERSION));
		data->set("srs_authors", RtmpAmf0Any::str(RTMP_SIG_SRS_AUTHORS));

		if (server_ip)
		{
			data->set("srs_server_ip", RtmpAmf0Any::str(server_ip));
		}
		// for edge to directly get the id of client.
		data->set("srs_pid", RtmpAmf0Any::number(getpid()));
		// data->set("srs_id", RtmpAmf0Any::str(_srs_context->get_id().c_str()));

		if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
		{
			return srs_error_wrap(err, "send connect app response");
		}

		return err;
	}

	srs_error_t RtmpTransport::HandleRtmpFMLEStartPacket(RtmpFMLEStartPacket* packet)
	{
		srs_error_t err = srs_success;

		RtmpRequest* req = info_->req;
		info_->type      = SrsRtmpConnFMLEPublish;
		req->stream      = packet->stream_name;
		MS_DEBUG_DEV(
		  "HandleRtmpFMLEStartPacket command=%s, transaction_id=%f, stream = %s",
		  packet->command_name.c_str(),
		  packet->transaction_id,
		  req->stream.c_str());

		{
			RtmpFMLEStartResPacket* pkt = new RtmpFMLEStartResPacket(packet->transaction_id);
			if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
			{
				return srs_error_wrap(err, "send releaseStream response");
			}
		}

		return err;
	}

	srs_error_t RtmpTransport::HandleRtmpCreateStreamPacket(RtmpCreateStreamPacket* packet)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV("HandleRtmpCreateStreamPacket transaction_id=%f", packet->transaction_id);
		if (true)
		{
			RtmpCreateStreamResPacket* pkt =
			  new RtmpCreateStreamResPacket(packet->transaction_id, info_->res->stream_id);
			if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
			{
				return srs_error_wrap(err, "send createStream response");
			}
		}
		return err;
	}

	srs_error_t RtmpTransport::HandleRtmpPublishPacket(RtmpPublishPacket* packet)
	{
		srs_error_t err = srs_success;

		// publish response onFCPublish(NetStream.Publish.Start)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->command_name = RTMP_AMF0_COMMAND_ON_FC_PUBLISH;
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodePublishStart));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Started publishing stream."));

			if ((err = connection_->send_and_free_packet(pkt, info_->res->stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Publish.Start");
			}
		}
		// publish response onStatus(NetStream.Publish.Start)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodePublishStart));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Started publishing stream."));
			pkt->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = connection_->send_and_free_packet(pkt, info_->res->stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Publish.Start");
			}
		}

		connection_->b_showDebugLog = false;

		// [dming] 到这里就完成publish的前期工作，进入直播流音视频和数据推送
		RtmpRouter* router = nullptr;
		if ((err = rtmpServer_->FetchOrCreateRouter(info_->req, &router)) != srs_success)
		{
			return srs_error_wrap(err, "rtmp: fetch router");
		}
		router->AddTransport(this);
		return err;
	}
} // namespace RTMP