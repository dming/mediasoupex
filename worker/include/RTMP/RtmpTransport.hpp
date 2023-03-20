#ifndef MS_RTMP_TRANSPORT_HPP
#define MS_RTMP_TRANSPORT_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpRouter.hpp"
#include "RTMP/RtmpSession.hpp"
#include "RTC/TransportTuple.hpp"

namespace RTMP
{
	class RtmpRouter;
	class RtmpTransport
	{
	private:
		/* data */
	public:
		RtmpTransport(RtmpRouter* router, RtmpSession* session);
		virtual ~RtmpTransport();

	public:
		RtmpSession* GetSession()
		{
			return session_;
		}
		srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg);
		srs_error_t send_and_free_message(RtmpSharedPtrMessage* msg, int stream_id);

	protected:
		virtual srs_error_t OnRecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) = 0;

	protected:
		RtmpSession* session_;
		RtmpRouter* router_;
	};

} // namespace RTMP

#endif