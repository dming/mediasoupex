#define MS_CLASS "RTMP::RtmpSession"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpSession.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpRouter.hpp"
#include "RTMP/RtmpServer.hpp"
#include "RTMP/RtmpTransport.hpp"

// FMLE
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH "onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH "onFCUnpublish"

namespace RTMP
{
	RtmpSession::RtmpSession(RtmpServer* rtmpServer)
	  : rtmpServer_(rtmpServer), connection_(new RTMP::RtmpTcpConnection(this, 65535)),
	    protocol_(new RtmpProtocol()), info_(new RtmpClientInfo()), transport_(nullptr)
	{
		MS_TRACE();
	}

	RtmpSession::~RtmpSession()
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("~RtmpSession");

		// NOTE: connection will be free in TcpServerHandler::OnTcpConnectionClosed
		FREEP(info_);
		FREEP(protocol_);
		connection_ = nullptr;
		rtmpServer_ = nullptr;
		transport_  = nullptr;
	}

	void RtmpSession::OnTcpConnectionPacketReceived(
	  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg)
	{
		MS_TRACE();
		std::lock_guard<std::mutex> lock(messageMutex);
		RTC::TransportTuple tuple(connection);
		if (transport_ != nullptr)
		{
			transport_->RecvMessage(&tuple, msg);
		}
		else
		{
			OnRecvMessage(&tuple, msg);
		}
	}

	srs_error_t RtmpSession::OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg)
	{
		MS_TRACE();

		srs_error_t err = srs_success;

		RTMP::RtmpPacket* packet;
		if (msg->header.is_user_control_message())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpUserControlPacket* m_packet = dynamic_cast<RtmpUserControlPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpUserControlPacket ");
				if (m_packet->event_type == SrcPCUCSetBufferLength)
				{
					protocol_->in_buffer_length = m_packet->extra_data;
				}
				if (m_packet->event_type == SrcPCUCPingRequest)
				{
					if ((err = response_ping_message(m_packet->event_data)) != srs_success)
					{
						return srs_error_wrap(err, "response ping");
					}
				}
			}
			else
			{
				MS_ERROR_STD("====>OnRecvMessage RtmpUserControlPacket ");
			}
		}
		else if (msg->header.is_window_ackledgement_size())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpSetWindowAckSizePacket* m_packet = dynamic_cast<RtmpSetWindowAckSizePacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpSetWindowAckSizePacket ");
				if (m_packet->ackowledgement_window_size > 0)
				{
					protocol_->in_ack_size.window = (uint32_t)m_packet->ackowledgement_window_size;
					// @remark, we ignore this message, for user noneed to care.
					// but it's important for dev, for client/server will block if required
					// ack msg not arrived.
				}
			}
			else
			{
				MS_ERROR_STD("====>OnRecvMessage RtmpSetWindowAckSizePacket ");
			}
		}
		else if (msg->header.is_set_chunk_size())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpSetChunkSizePacket* m_packet = dynamic_cast<RtmpSetChunkSizePacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpSetChunkSizePacket ");
				// for some server, the actual chunk size can greater than the max value(65536),
				// so we just warning the invalid chunk size, and actually use it is ok,
				// @see: https://github.com/ossrs/srs/issues/160
				if (m_packet->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE || m_packet->chunk_size > SRS_CONSTS_RTMP_MAX_CHUNK_SIZE)
				{
					MS_WARN_DEV(
					  "accept chunk=%d, should in [%d, %d], please see #160",
					  m_packet->chunk_size,
					  SRS_CONSTS_RTMP_MIN_CHUNK_SIZE,
					  SRS_CONSTS_RTMP_MAX_CHUNK_SIZE);
				}

				// @see: https://github.com/ossrs/srs/issues/541
				if (m_packet->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE)
				{
					return srs_error_new(
					  ERROR_RTMP_CHUNK_SIZE,
					  "chunk size should be %d+, value=%d",
					  SRS_CONSTS_RTMP_MIN_CHUNK_SIZE,
					  m_packet->chunk_size);
				}

				protocol_->in_chunk_size = m_packet->chunk_size;
			}
			else
			{
				MS_ERROR_STD("====>OnRecvMessage RtmpSetChunkSizePacket ");
			}
		}
		else if (msg->header.is_ackledgement())
		{
			MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpAcknowledgementPacket ");
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
		}
		else if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			if (RtmpConnectAppPacket* m_packet = dynamic_cast<RtmpConnectAppPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpConnectAppPacket ");
				return HandleRtmpConnectAppPacket(m_packet);
			}
			else if (RtmpCreateStreamPacket* m_packet = dynamic_cast<RtmpCreateStreamPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpCreateStreamPacket ");
				return HandleRtmpCreateStreamPacket(m_packet);
			}
			else if (RtmpPlayPacket* m_packet = dynamic_cast<RtmpPlayPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpPlayPacket ");
				return HandleRtmpPlayPacket(m_packet);
			}
			else if (RtmpPausePacket* m_packet = dynamic_cast<RtmpPausePacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpPausePacket ");
			}
			else if (RtmpFMLEStartPacket* m_packet = dynamic_cast<RtmpFMLEStartPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpFMLEStartPacket ");
				return HandleRtmpFMLEStartPacket(m_packet);
			}
			else if (RtmpPublishPacket* m_packet = dynamic_cast<RtmpPublishPacket*>(packet))
			{
				MS_DEBUG_DEV_STD(
				  "====>OnRecvMessage RtmpPublishPacket command name is %s ", m_packet->command_name.c_str());
				return HandleRtmpPublishPacket(m_packet);
			}
			else if (RtmpCloseStreamPacket* m_packet = dynamic_cast<RtmpCloseStreamPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpCloseStreamPacket ");
			}
			else if (RtmpCallPacket* m_packet = dynamic_cast<RtmpCallPacket*>(packet))
			{
				MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpCallPacket ");
				// only response it when transaction id not zero,
				// for the zero means donot need response.
				if (m_packet->transaction_id > 0)
				{
					RtmpCallResPacket* res = new RtmpCallResPacket(m_packet->transaction_id);
					res->command_object    = RtmpAmf0Any::null();
					res->response          = RtmpAmf0Any::null();
					if ((err = connection_->send_and_free_packet(res, 0)) != srs_success)
					{
						return srs_error_wrap(err, "rtmp: send packets");
					}
				}
			}
		}
		else if (msg->header.is_amf0_data() || msg->header.is_amf3_data())
		{
			if ((err = protocol_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
		}

		return err;
	}

	srs_error_t RtmpSession::HandleRtmpConnectAppPacket(RtmpConnectAppPacket* packet)
	{
		MS_TRACE();
		srs_error_t err = srs_success;

		RtmpAmf0Any* prop = nullptr;

		RtmpRequest* req = info_->req;

		if ((prop = packet->command_object->ensure_property_string("tcUrl")) == nullptr)
		{
			return srs_error_new(ERROR_RTMP_REQ_CONNECT, "invalid request without tcUrl");
		}
		req->tcUrl = prop->to_str(); // [dming] 客户端告诉服务器请求的url信息

		if ((prop = packet->command_object->ensure_property_string("pageUrl")) != nullptr)
		{
			req->pageUrl = prop->to_str();
		}

		if ((prop = packet->command_object->ensure_property_string("swfUrl")) != nullptr)
		{
			req->swfUrl = prop->to_str();
		}

		if ((prop = packet->command_object->ensure_property_number("objectEncoding")) != nullptr)
		{
			req->objectEncoding = prop->to_number();
		}

		// tcUrl=rtmp://192.168.167.88:19350/aaa
		MS_DEBUG_DEV_STD(
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
		MS_DEBUG_DEV_STD(
		  "[2] HandleRtmpConnectAppPacket schema=%s,  app=%s,stream=%s",
		  req->schema.c_str(),
		  req->app.c_str(),
		  req->stream.c_str());

		response_connect_app(req, connection_->GetLocalIp().c_str());

		return srs_success;
	}

	srs_error_t RtmpSession::response_connect_app(RtmpRequest* req, const char* server_ip)
	{
		srs_error_t err = srs_success;

		RtmpConnectAppResPacket* m_packet = new RtmpConnectAppResPacket();

		// @remark For windows, there must be a space between const string and macro.
		m_packet->props->set("fmsVer", RtmpAmf0Any::str("FMS/" RTMP_SIG_FMS_VER));
		m_packet->props->set("capabilities", RtmpAmf0Any::number(127));
		m_packet->props->set("mode", RtmpAmf0Any::number(1));

		m_packet->info->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
		m_packet->info->set(StatusCode, RtmpAmf0Any::str(StatusCodeConnectSuccess));
		m_packet->info->set(StatusDescription, RtmpAmf0Any::str("Connection succeeded"));
		m_packet->info->set("objectEncoding", RtmpAmf0Any::number(req->objectEncoding));
		RtmpAmf0EcmaArray* data = RtmpAmf0Any::ecma_array();
		m_packet->info->set("data", data);

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

		if ((err = connection_->send_and_free_packet(m_packet, 0)) != srs_success)
		{
			return srs_error_wrap(err, "send connect app response");
		}

		return err;
	}

	srs_error_t RtmpSession::HandleRtmpFMLEStartPacket(RtmpFMLEStartPacket* packet)
	{
		srs_error_t err = srs_success;

		RtmpRequest* req = info_->req;
		info_->type      = SrsRtmpConnFMLEPublish;
		req->stream      = packet->stream_name;
		MS_DEBUG_DEV_STD(
		  "HandleRtmpFMLEStartPacket command=%s, transaction_id=%f, stream=%s",
		  packet->command_name.c_str(),
		  packet->transaction_id,
		  req->stream.c_str());

		{
			RtmpFMLEStartResPacket* m_packet = new RtmpFMLEStartResPacket(packet->transaction_id);
			if ((err = connection_->send_and_free_packet(m_packet, 0)) != srs_success)
			{
				return srs_error_wrap(err, "send releaseStream response");
			}
		}

		return err;
	}

	srs_error_t RtmpSession::HandleRtmpCreateStreamPacket(RtmpCreateStreamPacket* packet)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV_STD("HandleRtmpCreateStreamPacket transaction_id=%f", packet->transaction_id);
		if (true)
		{
			RtmpCreateStreamResPacket* m_packet =
			  new RtmpCreateStreamResPacket(packet->transaction_id, info_->res->stream_id);
			if ((err = connection_->send_and_free_packet(m_packet, 0)) != srs_success)
			{
				return srs_error_wrap(err, "send createStream response");
			}
		}
		return err;
	}

	srs_error_t RtmpSession::HandleRtmpPublishPacket(RtmpPublishPacket* packet)
	{
		srs_error_t err = srs_success;

		// publish response onFCPublish(NetStream.Publish.Start)
		if (true)
		{
			RtmpOnStatusCallPacket* m_packet = new RtmpOnStatusCallPacket();

			m_packet->command_name = RTMP_AMF0_COMMAND_ON_FC_PUBLISH;
			m_packet->data->set(StatusCode, RtmpAmf0Any::str(StatusCodePublishStart));
			m_packet->data->set(StatusDescription, RtmpAmf0Any::str("Started publishing stream."));

			if ((err = connection_->send_and_free_packet(m_packet, info_->res->stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Publish.Start");
			}
		}
		// publish response onStatus(NetStream.Publish.Start)
		if (true)
		{
			RtmpOnStatusCallPacket* m_packet = new RtmpOnStatusCallPacket();

			m_packet->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			m_packet->data->set(StatusCode, RtmpAmf0Any::str(StatusCodePublishStart));
			m_packet->data->set(StatusDescription, RtmpAmf0Any::str("Started publishing stream."));
			m_packet->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = connection_->send_and_free_packet(m_packet, info_->res->stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Publish.Start");
			}
		}

		connection_->b_showDebugLog = false;

		// [dming] 到这里就完成publish的前期工作，进入直播流音视频和数据推送
		RtmpRouter* router       = nullptr;
		RtmpPublisher* publisher = nullptr;
		if ((err = rtmpServer_->FetchOrCreateRouter(info_->req, &router)) != srs_success)
		{
			MS_ERROR_STD("HandleRtmpPublishPacket cannot FetchOrCreateRouter, close the connection");
			connection_->Close();
			return srs_error_wrap(err, "rtmp: fetch router");
		}
		if (!router)
		{
			MS_ERROR("FUKKKKKKKKKKKK!!! router");
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "fuck");
		}
		MS_DEBUG_DEV_STD(
		  "HandleRtmpPublishPacket succeed FetchOrCreateRouter: %s",
		  info_->req->get_stream_url().c_str());
		if ((err = router->CreatePublisher(this, &publisher)) != srs_success)
		{
			MS_ERROR_STD("HandleRtmpPublishPacket cannot creare publisher, close the connection");
			connection_->Close();
			return srs_error_wrap(err, "rtmp: create publisher");
		}
		if (!publisher)
		{
			MS_ERROR("FUKKKKKKKKKKKK!!! publisher");
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "fuck");
		}

		MS_DEBUG_DEV_STD(
		  "HandleRtmpPublishPacket succeed CreatePublisher: %s", info_->req->get_stream_url().c_str());
		MS_DEBUG_DEV_STD(
		  "HandleRtmpPublishPacket succeed set publisher nooooo to transport_: %s",
		  info_->req->get_stream_url().c_str());

		transport_ = publisher;

		return err;
	}

	srs_error_t RtmpSession::response_ping_message(int32_t timestamp)
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
		if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
		{
			return srs_error_wrap(err, "ping response");
		}

		return err;
	}

	srs_error_t RtmpSession::HandleRtmpPlayPacket(RtmpPlayPacket* packet)
	{
		srs_error_t err      = srs_success;
		info_->type          = SrsRtmpConnPlay;
		info_->req->stream   = packet->stream_name;
		info_->req->duration = (srs_utime_t)packet->duration * SRS_UTIME_MILLISECONDS;

		int stream_id = info_->res->stream_id;
		// StreamBegin
		if (true)
		{
			RtmpUserControlPacket* pkt = new RtmpUserControlPacket();
			pkt->event_type            = SrcPCUCStreamBegin;
			pkt->event_data            = stream_id;
			if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
			{
				return srs_error_wrap(err, "send StreamBegin");
			}
		}

		// onStatus(NetStream.Play.Reset)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamReset));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Playing and resetting stream."));
			pkt->data->set(StatusDetails, RtmpAmf0Any::str("stream"));
			pkt->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Play.Reset");
			}
		}

		// onStatus(NetStream.Play.Start)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamStart));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Started playing stream."));
			pkt->data->set(StatusDetails, RtmpAmf0Any::str("stream"));
			pkt->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Play.Start");
			}
		}

		// |RtmpSampleAccess(false, false)
		if (true)
		{
			RtmpSampleAccessPacket* pkt = new RtmpSampleAccessPacket();

			// allow audio/video sample.
			// @see: https://github.com/ossrs/srs/issues/49
			pkt->audio_sample_access = true;
			pkt->video_sample_access = true;

			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send |RtmpSampleAccess true");
			}
		}

		// onStatus(NetStream.Data.Start)
		if (true)
		{
			RtmpOnStatusDataPacket* pkt = new RtmpOnStatusDataPacket();
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeDataStart));
			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Data.Start");
			}
		}

		connection_->b_showDebugLog = false;

		// [dming] 到这里就完成play的前期工作，进入直播流音视频和数据接收
		RtmpRouter* router     = nullptr;
		RtmpConsumer* consumer = nullptr;
		if ((err = rtmpServer_->FetchOrCreateRouter(info_->req, &router)) != srs_success)
		{
			MS_ERROR_STD("HandleRtmpPlayPacket cannot FetchOrCreateRouter, close the connection");
			connection_->Close();
			return srs_error_wrap(err, "rtmp: fetch router");
		}
		if ((err = router->CreateConsumer(this, consumer)) != srs_success)
		{
			MS_ERROR_STD("HandleRtmpPlayPacket cannot creare consumer, close the connection");
			connection_->Close();
			return srs_error_wrap(err, "rtmp: create consumer");
		}
		router->ConsumerDump(consumer);
		transport_ = consumer;
		return srs_success;
	}
} // namespace RTMP