#define MS_CLASS "RTMP::RtmpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpServer.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/TransportTuple.hpp"
#include <stdexcept>

namespace RTMP
{
	/* Instance methods. */

	RtmpServer::RtmpServer(RTC::Shared* shared, const std::string& id, json& data)
	  : id(id), shared(shared)
	{
		MS_TRACE();

		auto jsonListenInfoIt = data.find("listenInfo");
		if (jsonListenInfoIt == data.end())
			MS_THROW_TYPE_ERROR("missing listenInfo 2");
		else if (!jsonListenInfoIt->is_object())
			MS_THROW_TYPE_ERROR("wrong listenInfo (not an object)");

		auto& jsonListenInfo = *jsonListenInfoIt;

		std::string listenIp;

		// data format: RtmpServerListenInfo:
		auto jsonListenIpIt = jsonListenInfo.find("listenIp");
		if (jsonListenIpIt == jsonListenInfo.end())
			MS_THROW_TYPE_ERROR("missing listenIp 2");
		else if (!jsonListenIpIt->is_string())
			MS_THROW_TYPE_ERROR("wrong listenIp (not an string)");

		listenIp = jsonListenIpIt->get<std::string>();

		// This may throw.
		Utils::IP::NormalizeIp(listenIp);

		uint16_t port{ 0 };

		auto jsonPortIt = jsonListenInfo.find("port");

		if (jsonPortIt != jsonListenInfo.end())
		{
			if (!(jsonPortIt->is_number() && Utils::Json::IsPositiveInteger(*jsonPortIt)))
				MS_THROW_TYPE_ERROR("wrong port (not a positive number)");

			port = jsonPortIt->get<uint16_t>();
		}

		if (port != 0)
			tcpServer = new RTMP::RtmpTcpServer(this, listenIp, port);
		else
			tcpServer = new RTMP::RtmpTcpServer(this, listenIp);

		this->shared->channelMessageRegistrator->RegisterHandler(
		  this->id,
		  /*channelRequestHandler*/ this,
		  /*payloadChannelRequestHandler*/ nullptr,
		  /*payloadChannelNotificationHandler*/ nullptr);
	}

	RtmpServer::~RtmpServer()
	{
		MS_TRACE();
		// unregister shared
		this->shared->channelMessageRegistrator->UnregisterHandler(this->id);
		// clear tcpserver
		FREEP(tcpServer);
	}

	void RtmpServer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();
	}

	inline RtmpSession* RtmpServer::CreateNewSession()
	{
		return new RtmpSession(this);
	}

	inline void RtmpServer::OnRtcTcpConnectionClosed(
	  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection)
	{
		MS_TRACE();
		RTC::TransportTuple tuple(connection);
		MS_DEBUG_DEV_STD("OnRtcTcpConnectionClosed hash=%" PRIu64, tuple.hash);
		std::map<uint64_t, RTMP::RtmpSession*>::iterator sessionIt = sessions_.find(tuple.hash);
		if (sessionIt != sessions_.end())
		{
			sessions_.erase(sessionIt);
			RtmpSession* session = sessionIt->second;
			RtmpRouter* router   = FetchRouter(session->GetStreamUrl());
			if (router)
			{
				MS_DEBUG_DEV_STD(
				  "OnRtcTcpConnectionClosed fetch router of url:=%s", session->GetStreamUrl().c_str());
				router->RemoveSession(session);
			}
			MS_DEBUG_DEV_STD(
			  "OnRtcTcpConnectionClosed FREEP(session) of url:=%s", session->GetStreamUrl().c_str());
			FREEP(session);
			return;
		}
	}

	inline void RtmpServer::OnRtmpSessionCreated(RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpSession* session)
	{
		MS_TRACE();
		RTMP::RtmpTcpConnection* connection = session->GetConnection();
		MS_ASSERT(connection != nullptr, "session should has connection");
		RTC::TransportTuple tuple(connection);
		MS_DEBUG_DEV_STD("OnRtmpSessionCreated, hash=%" PRIu64, tuple.hash);

		MS_ASSERT(
		  sessions_.find(tuple.hash) == sessions_.end(),
		  "cannot dumplicate create RtmpSession: [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "]",
		  connection->GetLocalIp().c_str(),
		  connection->GetLocalPort(),
		  connection->GetPeerIp().c_str(),
		  connection->GetPeerPort());

		sessions_[tuple.hash] = session;
	}

	RtmpRouter* RtmpServer::FetchRouter(RtmpRequest* req)
	{
		std::string streamUrl = req->get_stream_url();
		return FetchRouter(streamUrl);
	}

	RtmpRouter* RtmpServer::FetchRouter(std::string streamUrl)
	{
		RtmpRouter* router = nullptr;
		if (routers_.find(streamUrl) == routers_.end())
		{
			return nullptr;
		}
		router = routers_[streamUrl];
		return router;
	}

	srs_error_t RtmpServer::FetchOrCreateRouter(RtmpRequest* req, RtmpRouter** pps)
	{
		srs_error_t err       = srs_success;
		std::string streamUrl = req->get_stream_url();
		if (req->stream.empty())
		{
			MS_ERROR_STD("empty stream, url=%s", streamUrl.c_str());
			return srs_error_new(ERROR_RTMP_STREAM_NAME_EMPTY, "rtmp: empty stream");
		}

		MS_DEBUG_DEV_STD("FetchOrCreateRouter streamUrl is %s", streamUrl.c_str());
		RtmpRouter* router = nullptr;
		if (router = FetchRouter(streamUrl))
		{
			// we always update the request of resource,
			// for origin auth is on, the token in request maybe invalid,
			// and we only need to update the token of request, it's simple.
			// router->update_auth(r);
			*pps = router;
			return err;
		}

		// should always not exists for create a source.
		srs_assert(routers_.find(streamUrl) == routers_.end());

		MS_DEBUG_DEV_STD("new live Router, stream_url=%s", streamUrl.c_str());

		try
		{
			router = new RtmpRouter();
			if ((err = router->initualize(req)) != srs_success)
			{
				MS_ERROR_STD("ERROR Router initualize, stream_url=%s", streamUrl.c_str());
				err = srs_error_wrap(err, "init Router %s", streamUrl.c_str());
				FREEP(router);
				return err;
			}

			routers_[streamUrl] = router;
			*pps                = router;
			return err;
		}
		catch (std::exception& e)
		{
			MS_ERROR_STD("ERROR Router FUCKKKKKK!!, stream_url=%s", streamUrl.c_str());
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "ERROR Router FUCKKKKKK!!");
		}
	}

} // namespace RTMP