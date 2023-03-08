#define MS_CLASS "RTMP::RtmpRouter"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpRouter.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTMP/RtmpTransport.hpp"
#include "RTC/TransportTuple.hpp"

namespace RTMP
{
	RtmpRouter::RtmpRouter(/* args */)
	{
	}

	RtmpRouter::~RtmpRouter()
	{
		FREEP(req_);
	}

	srs_error_t RtmpRouter::initualize(RtmpRequest* req)
	{
		srs_error_t err = srs_success;
		req_            = req->copy();
		return err;
	}

	srs_error_t RtmpRouter::AddTransport(RtmpTransport* transport)
	{
		srs_error_t err               = srs_success;
		RtmpTcpConnection* connection = transport->GetConnection();
		RTC::TransportTuple tuple(connection);

		if (transports_.find(tuple.hash) != transports_.end())
		{
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "transport already in router");
		}
		if (transport->IsPublisher() && publisher_ != nullptr)
		{
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "publisher_ already in router");
		}

		MS_DEBUG_DEV(
		  "AddTransport tuple.hash=%" PRIu64 ", isPublisher=%d", tuple.hash, transport->IsPublisher());

		transports_[tuple.hash] = transport;
		if (transport->IsPublisher())
		{
			publisher_ = transport;
		}
		return err;
	}

	srs_error_t RtmpRouter::RemoveTransport(RtmpTransport* transport)
	{
		srs_error_t err               = srs_success;
		RtmpTcpConnection* connection = transport->GetConnection();
		RTC::TransportTuple tuple(connection);
		if (transports_.find(tuple.hash) == transports_.end())
		{
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "transport not exist");
		}
		MS_DEBUG_DEV(
		  "RemoveTransport tuple.hash=%" PRIu64 ", isPublisher=%d", tuple.hash, transport->IsPublisher());

		transports_.erase(tuple.hash);
		if (transport->IsPublisher())
		{
			publisher_ = nullptr;
		}
		return err;
	}
} // namespace RTMP
