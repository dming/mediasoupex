#ifndef MS_RTMP_SERVER_TRANSPORT_HPP
#define MS_RTMP_SERVER_TRANSPORT_HPP

#include "RTMP/RtmpServerSession.hpp"
#include "RTMP/RtmpTransport.hpp"

namespace RTMP
{
	class RtmpServerTransport : public RtmpTransport, public RTMP::RtmpServerSession::Listener
	{
	public:
		explicit RtmpServerTransport(
		  RTC::Shared* shared,
		  std::string id,
		  RtmpTransport::Listener* listener,
		  bool IsPublisher,
		  RtmpServerSession* session); //  Listener为router

		virtual ~RtmpServerTransport();

	public:
		void FillJson(json& jsonObject) const override;

		/* Pure virtual methods inherited from IRtmpTransport. */
	public:
		RtmpRtmpConnType GetInfoType() override;
		int GetStreamId() override;
		srs_error_t OnDecodeMessage(RtmpCommonMessage* msg, RtmpPacket** ppacket) override;
		srs_error_t OnSendAndFreeMessage(RtmpSharedPtrMessage* msg, int stream_id) override;
		srs_error_t OnSendAndFreePacket(RtmpPacket* packet, int stream_id) override;

		/* Pure virtual methods inherited from RTMP::RtmpServerSession::Listener. */
	public:
		srs_error_t RecvMessage(RTC::TransportTuple* tuple, RtmpCommonMessage* msg) override;

	public:
		RtmpServerSession* GetSession()
		{
			return session_;
		}

	private:
		RtmpServerSession* session_;
	};
} // namespace RTMP
#endif