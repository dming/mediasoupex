#define MS_CLASS "RTMP::RtmpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpServer.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"
#include "RTC/TransportTuple.hpp"

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
	}

	void RtmpServer::HandleRequest(Channel::ChannelRequest* request)
	{
		MS_TRACE();
	}

	inline RtmpTransport* RtmpServer::CreateNewTransport()
	{
		return new RtmpTransport(this);
	}

	inline void RtmpServer::OnRtcTcpConnectionClosed(
	  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection)
	{
		MS_TRACE();
		RTC::TransportTuple tuple(connection);
		std::map<uint64_t, RTMP::RtmpTransport*>::iterator transportIt = transports_.find(tuple.hash);
		if (transportIt != transports_.end())
		{
			transports_.erase(transportIt);
			RtmpTransport* transport = transportIt->second;
			// [dming] TODO: need erase the transport in router
			RtmpRouter* router = FetchRouter(transport->GetStreamUrl());
			if (router)
			{
				router->RemoveTransport(transport);
			}
			FREEP(transport);
		}
	}

	inline void RtmpServer::OnRtmpTransportCreated(
	  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTransport* transport)
	{
		MS_TRACE();
		MS_DEBUG_DEV("OnRtmpTransportCreated ");
		RTMP::RtmpTcpConnection* connection = transport->GetConnection();
		MS_ASSERT(connection != nullptr, "transport should has connection");
		RTC::TransportTuple tuple(connection);

		MS_ASSERT(
		  transports_.find(tuple.hash) == transports_.end(),
		  "cannot dumplicate create RtmpTransport: [local:%s :%" PRIu16 ", remote:%s :%" PRIu16 "]",
		  connection->GetLocalIp().c_str(),
		  connection->GetLocalPort(),
		  connection->GetPeerIp().c_str(),
		  connection->GetPeerPort());

		transports_[tuple.hash] = transport;
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
		srs_error_t err = srs_success;
		// if (req->stream.empty())
		// {
		// 	return srs_error_new(ERROR_RTMP_STREAM_NAME_EMPTY, "rtmp: empty stream");
		// }

		RtmpRouter* router = nullptr;
		if ((router = FetchRouter(req)) != nullptr)
		{
			// we always update the request of resource,
			// for origin auth is on, the token in request maybe invalid,
			// and we only need to update the token of request, it's simple.
			// router->update_auth(r);
			*pps = router;
			return err;
		}

		std::string streamUrl = req->get_stream_url();
		// should always not exists for create a source.
		srs_assert(routers_.find(streamUrl) == routers_.end());

		MS_DEBUG_DEV("new live Router, stream_url=%s", streamUrl.c_str());

		router = new RtmpRouter();
		if ((err = router->initualize(req)) != srs_success)
		{
			err = srs_error_wrap(err, "init Router %s", streamUrl.c_str());
			goto failed;
		}

		routers_[streamUrl] = router;
		*pps                = router;
		return err;

	failed:
		FREEP(router);
		return err;
	}

} // namespace RTMP