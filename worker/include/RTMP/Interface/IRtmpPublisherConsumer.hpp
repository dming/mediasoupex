#ifndef MS_I_RTMP_PUBLISHER_CONSUMER_HPP
#define MS_I_RTMP_PUBLISHER_CONSUMER_HPP

#include "RTMP/RtmpInfo.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"
#include "RTC/TransportTuple.hpp"

namespace RTMP
{
	class IRtmpPublisherConsumer
	{
	public:
		class Listener
		{
		public:
			virtual ~Listener()                                                                = default;
			virtual RtmpRtmpConnType GetInfoType()                                             = 0;
			virtual int GetStreamId()                                                          = 0;
			virtual srs_error_t OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket)  = 0;
			virtual srs_error_t OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id) = 0;
			virtual srs_error_t OnSendAndFreePacket(RtmpPacket* packet, int stream_id)         = 0;
		};

	public:
		virtual ~IRtmpPublisherConsumer() = default;

	public:
		virtual srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) = 0;
	};

} // namespace RTMP

#endif