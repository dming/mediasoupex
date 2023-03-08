#ifndef MS_RTMP_ROUTER_HPP
#define MS_RTMP_ROUTER_HPP

#include "CplxError.hpp"
#include <stdint.h>
#include <unordered_map>

namespace RTMP
{
	class RtmpTransport;
	class RtmpRequest;
	class RtmpRouter
	{
	public:
		RtmpRouter(/* args */);
		~RtmpRouter();

		virtual srs_error_t initualize(RtmpRequest* req);
		virtual srs_error_t AddTransport(RtmpTransport* transport);
		virtual srs_error_t RemoveTransport(RtmpTransport* transport);

	private:
		RtmpRequest* req_;
		RtmpTransport* publisher_;
		std::unordered_map<uint64_t, RtmpTransport*> transports_;
	};

} // namespace RTMP

#endif