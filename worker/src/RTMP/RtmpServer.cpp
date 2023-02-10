#define MS_CLASS "RTMP::RtmpServer"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpServer.hpp"
#include "Logger.hpp"
#include "MediaSoupErrors.hpp"
#include "Utils.hpp"

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
			tcpServer = new RTMP::RtmpTcpServer(this, this, listenIp, port);
		else
			tcpServer = new RTMP::RtmpTcpServer(this, this, listenIp);

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

	void RtmpServer::OnRtcTcpConnectionClosed(
	  RTMP::RtmpTcpServer* tcpServer, RTMP::RtmpTcpConnection* connection)
	{
		MS_TRACE();
	}

	void RtmpServer::OnTcpConnectionPacketReceived(
	  RTMP::RtmpTcpConnection* connection, const uint8_t* data, size_t len)
	{
		MS_TRACE();
	}
} // namespace RTMP