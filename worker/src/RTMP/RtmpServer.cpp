#define MS_CLASS "RTMP::RtmpServer"

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
        /**
         * data interface:
         * {
         *  listenIp: string,
         *  port: number
         * }
         */

        std::string ip;

        auto jsonListenIpIt = data.find("listenIp");
        if (jsonListenIpIt == data.end())
            MS_THROW_TYPE_ERROR("missing listenIp");
        else if (!jsonListenIpIt->is_string())
            MS_THROW_TYPE_ERROR("wrong listenIp (not an string)");
        
        ip = jsonListenIpIt->get<std::string>();

        // This may throw.
        Utils::IP::NormalizeIp(ip);

        uint16_t port{0};
        auto jsonPortIt = data.find("port");
        if (jsonPortIt != data.end()) 
        {
            if (!(jsonPortIt->is_number() && Utils::Json::IsPositiveInteger(*jsonPortIt)))
                MS_THROW_TYPE_ERROR("wrong port (not a positive number)");
            
            port = jsonPortIt->get<uint16_t>();
        }

        if (port != 0)
            tcpServer = new RTMP::RtmpTcpServer(this, this, ip, port);
        else 
            tcpServer = new RTMP::RtmpTcpServer(this, this, ip);
        
        this->shared->channelMessageRegistrator->RegisterHandler(
            this->id,
            /*channelRequestHandler*/ this,
            /*payloadChannelRequestHandler*/ nullptr,
            /*payloadChannelNotificationHandler*/ nullptr);
	}

    RtmpServer::~RtmpServer()
    {
        // unregister shared
        this->shared->channelMessageRegistrator->UnregisterHandler(this->id);
        // clear tcpserver
    }

    void RtmpServer::HandleRequest(Channel::ChannelRequest* request)
    {
        
    }
}