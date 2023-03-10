#ifndef MS_RTMP_PROTOCOL_HPP
#define MS_RTMP_PROTOCOL_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"
#include "UtilsBuffer.hpp"
#include <map>
#include <string>

namespace RTMP
{
	// The original request from client.
	class RtmpRequest
	{
	public:
		// The client ip.
		std::string ip;

	public:
		// The tcUrl: rtmp://request_vhost:port/app/stream
		// support pass vhost in query string, such as:
		//    rtmp://ip:port/app?vhost=request_vhost/stream
		//    rtmp://ip:port/app...vhost...request_vhost/stream
		std::string tcUrl;
		std::string pageUrl;
		std::string swfUrl;
		double objectEncoding;
		// The data discovery from request.
	public:
		// Discovery from tcUrl and play/publish.
		std::string schema;
		// The vhost in tcUrl.
		std::string vhost;
		// The host in tcUrl.
		std::string host;
		// The port in tcUrl.
		int port;
		// The app in tcUrl, without param.
		std::string app;
		// The param in tcUrl(app).
		std::string param;
		// The stream in play/publish
		std::string stream;
		// For play live stream,
		// used to specified the stop when exceed the duration.
		// in srs_utime_t.
		srs_utime_t duration;
		// The token in the connect request,
		// used for edge traverse to origin authentication,
		// @see https://github.com/ossrs/srs/issues/104
		RtmpAmf0Object* args;

	public:
		RtmpRequest();
		virtual ~RtmpRequest();

	public:
		// Deep copy the request, for source to use it to support reload,
		// For when initialize the source, the request is valid,
		// When reload it, the request maybe invalid, so need to copy it.
		virtual RtmpRequest* copy();
		// update the auth info of request,
		// To keep the current request ptr is ok,
		// For many components use the ptr of request.
		virtual void update_auth(RtmpRequest* req);
		// Get the stream identify, vhost/app/stream.
		std::string get_stream_url();
		// To strip url, user must strip when update the url.
		virtual void strip();

	public:
		// Transform it as HTTP request.
		virtual RtmpRequest* as_http();

	public:
		// The protocol of client:
		//      rtmp, Adobe RTMP protocol.
		//      flv, HTTP-FLV protocol.
		//      flvs, HTTPS-FLV protocol.
		std::string protocol;
	};

	// The response to client.
	class RtmpResponse
	{
	public:
		// The stream id to response client createStream.
		int stream_id;

	public:
		RtmpResponse();
		virtual ~RtmpResponse();
	};

	// The rtmp client type.
	enum SrsRtmpConnType
	{
		SrsRtmpConnUnknown = 0x0000,
		// All players.
		SrsRtmpConnPlay = 0x0100,
		SrsHlsPlay      = 0x0101,
		SrsFlvPlay      = 0x0102,
		SrsRtcConnPlay  = 0x0110,
		SrsSrtConnPlay  = 0x0120,
		// All publishers.
		SrsRtmpConnFMLEPublish      = 0x0200,
		SrsRtmpConnFlashPublish     = 0x0201,
		SrsRtmpConnHaivisionPublish = 0x0202,
		SrsRtcConnPublish           = 0x0210,
		SrsSrtConnPublish           = 0x0220,
	};
	std::string srs_client_type_string(SrsRtmpConnType type);
	bool srs_client_type_is_publish(SrsRtmpConnType type);

	// Some information of client.
	class RtmpClientInfo
	{
	public:
		// The type of client, play or publish.
		SrsRtmpConnType type;
		// Whether the client connected at the edge server.
		bool edge;
		// Original request object from client.
		RtmpRequest* req;
		// Response object to client.
		RtmpResponse* res;

	public:
		RtmpClientInfo();
		virtual ~RtmpClientInfo();

		bool IsPublisher()
		{
			return type == SrsRtmpConnFMLEPublish || type == SrsRtmpConnFlashPublish ||
			       type == SrsRtmpConnHaivisionPublish;
		}
	};

	class RtmpProtocol
	{
	private:
		class AckWindowSize
		{
		public:
			uint32_t window;
			// number of received bytes.
			int64_t nb_recv_bytes;
			// previous responsed sequence number.
			uint32_t sequence_number;

			AckWindowSize();
		};

	public:
		RtmpProtocol(/* args */);
		~RtmpProtocol();

	public:
		// Decode bytes oriented RTMP message to RTMP packet,
		// @param ppacket, output decoded packet,
		//       always nullptr if error, never nullptr if success.
		// @return error when unknown packet, error when decode failed.
		virtual srs_error_t decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket);
		// When message sentout, update the context.
		virtual srs_error_t on_send_packet(RtmpMessageHeader* mh, RtmpPacket* packet);

	private:
		// The imp for decode_message
		virtual srs_error_t do_decode_message(
		  RtmpMessageHeader& header, Utils::RtmpBuffer* stream, RtmpPacket** ppacket);

		void print_debug_info();

	public:
		// The requests sent out, used to build the response.
		// key: transactionId
		// value: the request command name
		std::map<double, std::string> requests;
		// The output chunk size, default to 128, set by config.
		int32_t out_chunk_size;
		// The input ack window, to response acknowledge to peer,
		// For example, to respose the encoder, for server got lots of packets.
		AckWindowSize in_ack_size;
		// The output ack window, to require peer to response the ack.
		AckWindowSize out_ack_size;
		// The buffer length set by peer.
		int32_t in_buffer_length;
		// The input chunk size, default to 128, set by peer packet.
		int32_t in_chunk_size;
		// Whether print the protocol level debug info.
		// Generally we print the debug info when got or send first A/V packet.
		bool show_debug_info;
	};

} // namespace RTMP

#endif