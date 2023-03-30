#ifndef MS_RTMP_INFO_HPP
#define MS_RTMP_INFO_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"
#include "UtilsBuffer.hpp"
#include <map>
#include <string>

// default stream id for response the createStream request.
#define SRS_DEFAULT_SID 1

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

	public:
		RtmpResponse* copy()
		{
			RtmpResponse* cp = new RtmpResponse();
			cp->stream_id    = this->stream_id;
			return cp;
		}
	};

	// The rtmp client type.
	enum RtmpRtmpConnType
	{
		RtmpRtmpConnUnknown = 0x0000,
		// All players.
		RtmpRtmpConnPlay = 0x0100,
		RtmpHlsPlay      = 0x0101,
		RtmpFlvPlay      = 0x0102,
		RtmpRtcConnPlay  = 0x0110,
		RtmpSrtConnPlay  = 0x0120,
		// All publishers.
		RtmpRtmpConnFMLEPublish      = 0x0200,
		RtmpRtmpConnFlashPublish     = 0x0201,
		RtmpRtmpConnHaivisionPublish = 0x0202,
		RtmpRtcConnPublish           = 0x0210,
		RtmpSrtConnPublish           = 0x0220,
	};
	std::string srs_client_type_string(RtmpRtmpConnType type);
	bool srs_client_type_is_publish(RtmpRtmpConnType type);

	// Some information of client.
	class RtmpClientInfo
	{
	public:
		// The type of client, play or publish.
		RtmpRtmpConnType type;
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
			return type == RtmpRtmpConnFMLEPublish || type == RtmpRtmpConnFlashPublish ||
			       type == RtmpRtmpConnHaivisionPublish;
		}

		RtmpClientInfo* copy()
		{
			RtmpClientInfo* cp = new RtmpClientInfo();
			cp->type           = this->type;
			cp->edge           = this->edge;
			cp->req            = this->req->copy();
			cp->res            = this->res->copy();
			return cp;
		}
	};

} // namespace RTMP

#endif