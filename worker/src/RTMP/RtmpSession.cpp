#define MS_CLASS "RTMP::RtmpSession"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpSession.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpConsumer.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpPublisher.hpp"
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
	    info_(new RtmpClientInfo()), transport_(nullptr)
	{
		MS_TRACE();
	}

	RtmpSession::~RtmpSession()
	{
		MS_TRACE();
		MS_DEBUG_DEV_STD("~RtmpSession");

		// NOTE: connection will be free in TcpServerHandler::OnTcpConnectionClosed
		FREEP(info_);
		connection_ = nullptr;
		rtmpServer_ = nullptr;
		transport_  = nullptr;
	}

	void RtmpSession::OnTcpConnectionPacketReceived(
	  RTMP::RtmpTcpConnection* connection, RtmpCommonMessage* msg)
	{
		MS_TRACE();
		// std::lock_guard<std::mutex> lock(messageMutex);
		RTC::TransportTuple tuple(connection);
		if (transport_)
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

		if (msg->header.is_ackledgement())
		{
			MS_DEBUG_DEV_STD("====>OnRecvMessage RtmpAcknowledgementPacket ");
		}
		else if (msg->header.is_amf0_command() || msg->header.is_amf3_command())
		{
			RtmpPacket* packet;
			if ((err = connection_->decode_message(msg, &packet)) != srs_success)
			{
				return srs_error_wrap(err, "decode message");
			}
			RtmpAutoFree(RtmpPacket, packet);
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
				MS_DEBUG_DEV_STD(
				  "====>OnRecvMessage RtmpCallPacket command name is:", m_packet->command_name.c_str());
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
			// do nothing
			MS_ERROR("OnRecvMessage should not get amf0/3 data");
		}
		else if (msg->header.is_video() || msg->header.is_audio())
		{
			// do nothing
			MS_ERROR("OnRecvMessage should not get audio/video");
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
		info_->type      = RtmpRtmpConnFMLEPublish;
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
		if ((err = router->CreatePublisher(this, &publisher)) != srs_success)
		{
			MS_ERROR_STD("HandleRtmpPublishPacket cannot creare publisher, close the connection");
			connection_->Close();
			return srs_error_wrap(err, "rtmp: create publisher");
		}

		MS_DEBUG_DEV_STD(
		  "HandleRtmpPublishPacket succeed CreatePublisher: %s", info_->req->get_stream_url().c_str());

		transport_ = publisher;

		return err;
	}

	srs_error_t RtmpSession::HandleRtmpPlayPacket(RtmpPlayPacket* packet)
	{
		srs_error_t err      = srs_success;
		info_->type          = RtmpRtmpConnPlay;
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

	srs_error_t RtmpSession::on_play_client_pause(int stream_id, bool is_pause)
	{
		srs_error_t err = srs_success;

		if (is_pause)
		{
			// onStatus(NetStream.Pause.Notify)
			if (true)
			{
				RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

				pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
				pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamPause));
				pkt->data->set(StatusDescription, RtmpAmf0Any::str("Paused stream."));

				if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
				{
					return srs_error_wrap(err, "send NetStream.Pause.Notify");
				}
			}
			// StreamEOF
			if (true)
			{
				RtmpUserControlPacket* pkt = new RtmpUserControlPacket();

				pkt->event_type = SrcPCUCStreamEOF;
				pkt->event_data = stream_id;

				if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
				{
					return srs_error_wrap(err, "send StreamEOF");
				}
			}
		}
		else
		{
			// onStatus(NetStream.Unpause.Notify)
			if (true)
			{
				RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

				pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
				pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeStreamUnpause));
				pkt->data->set(StatusDescription, RtmpAmf0Any::str("Unpaused stream."));

				if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
				{
					return srs_error_wrap(err, "send NetStream.Unpause.Notify");
				}
			}
			// StreamBegin
			if (true)
			{
				RtmpUserControlPacket* pkt = new RtmpUserControlPacket();

				pkt->event_type = SrcPCUCStreamBegin;
				pkt->event_data = stream_id;

				if ((err = connection_->send_and_free_packet(pkt, 0)) != srs_success)
				{
					return srs_error_wrap(err, "send StreamBegin");
				}
			}
		}

		return err;
	}

	srs_error_t RtmpSession::fmle_unpublish(int stream_id, double unpublish_tid)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV_STD("fmle_unpublish stream_id=%d, unpublish_tid=%f", stream_id, unpublish_tid);
		// publish response onFCUnpublish(NetStream.unpublish.Success)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->command_name = RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH;
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeUnpublishSuccess));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Stop publishing stream."));

			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.unpublish.Success");
			}
			MS_DEBUG_DEV_STD("send NetStream.unpublish.Success");
		}
		// FCUnpublish response
		if (true)
		{
			RtmpFMLEStartResPacket* pkt = new RtmpFMLEStartResPacket(unpublish_tid);
			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send FCUnpublish response");
			}
			MS_DEBUG_DEV_STD("send FCUnpublish response");
		}
		// publish response onStatus(NetStream.Unpublish.Success)
		if (true)
		{
			RtmpOnStatusCallPacket* pkt = new RtmpOnStatusCallPacket();

			pkt->data->set(StatusLevel, RtmpAmf0Any::str(StatusLevelStatus));
			pkt->data->set(StatusCode, RtmpAmf0Any::str(StatusCodeUnpublishSuccess));
			pkt->data->set(StatusDescription, RtmpAmf0Any::str("Stream is now unpublished"));
			pkt->data->set(StatusClientId, RtmpAmf0Any::str(RTMP_SIG_CLIENT_ID));

			if ((err = connection_->send_and_free_packet(pkt, stream_id)) != srs_success)
			{
				return srs_error_wrap(err, "send NetStream.Unpublish.Success");
			}
			MS_DEBUG_DEV_STD("send NetStream.Unpublish.Success");
		}

		return err;
	}
} // namespace RTMP