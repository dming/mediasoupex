#define MS_CLASS "RTMP::RtmpInfo"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpInfo.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpUtility.hpp"

// FMLE
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH "onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH "onFCUnpublish"

// default stream id for response the createStream request.
#define SRS_DEFAULT_SID 1

namespace RTMP
{

	RtmpRequest::RtmpRequest()
	{
		objectEncoding = RTMP_SIG_AMF0_VER;
		duration       = -1;
		port           = SRS_CONSTS_RTMP_DEFAULT_PORT;
		args           = nullptr;

		protocol = "rtmp";
	}

	RtmpRequest::~RtmpRequest()
	{
		FREEP(args);
	}

	RtmpRequest* RtmpRequest::copy()
	{
		RtmpRequest* cp = new RtmpRequest();

		cp->ip             = ip;
		cp->vhost          = vhost;
		cp->app            = app;
		cp->objectEncoding = objectEncoding;
		cp->pageUrl        = pageUrl;
		cp->host           = host;
		cp->port           = port;
		cp->param          = param;
		cp->schema         = schema;
		cp->stream         = stream;
		cp->swfUrl         = swfUrl;
		cp->tcUrl          = tcUrl;
		cp->duration       = duration;
		if (args)
		{
			cp->args = args->copy()->to_object();
		}

		cp->protocol = protocol;

		return cp;
	}

	void RtmpRequest::update_auth(RtmpRequest* req)
	{
		pageUrl = req->pageUrl;
		swfUrl  = req->swfUrl;
		tcUrl   = req->tcUrl;
		param   = req->param;

		ip             = req->ip;
		vhost          = req->vhost;
		app            = req->app;
		objectEncoding = req->objectEncoding;
		host           = req->host;
		port           = req->port;
		param          = req->param;
		schema         = req->schema;
		duration       = req->duration;

		if (args)
		{
			FREEP(args);
		}
		if (req->args)
		{
			args = req->args->copy()->to_object();
		}

		protocol = req->protocol;

		MS_DEBUG_DEV_STD("update req of soruce for auth ok");
	}

	std::string RtmpRequest::get_stream_url()
	{
		return srs_generate_stream_url(vhost, app, stream);
	}

	void RtmpRequest::strip()
	{
		// remove the unsupported chars in names.
		host   = srs_string_remove(host, "/ \n\r\t");
		vhost  = srs_string_remove(vhost, "/ \n\r\t");
		app    = srs_string_remove(app, " \n\r\t");
		stream = srs_string_remove(stream, " \n\r\t");

		// remove end slash of app/stream
		app    = srs_string_trim_end(app, "/");
		stream = srs_string_trim_end(stream, "/");

		// remove start slash of app/stream
		app    = srs_string_trim_start(app, "/");
		stream = srs_string_trim_start(stream, "/");
	}

	RtmpRequest* RtmpRequest::as_http()
	{
		schema = "http";
		tcUrl  = srs_generate_tc_url(schema, host, vhost, app, port);
		return this;
	}

	RtmpResponse::RtmpResponse()
	{
		stream_id = SRS_DEFAULT_SID;
	}

	RtmpResponse::~RtmpResponse()
	{
	}

	std::string srs_client_type_string(RtmpRtmpConnType type)
	{
		switch (type)
		{
			case RtmpRtmpConnPlay:
				return "rtmp-play";
			case RtmpHlsPlay:
				return "hls-play";
			case RtmpFlvPlay:
				return "flv-play";
			case RtmpRtcConnPlay:
				return "rtc-play";
			case RtmpRtmpConnFlashPublish:
				return "flash-publish";
			case RtmpRtmpConnFMLEPublish:
				return "fmle-publish";
			case RtmpRtmpConnHaivisionPublish:
				return "haivision-publish";
			case RtmpRtcConnPublish:
				return "rtc-publish";
			case RtmpSrtConnPlay:
				return "srt-play";
			case RtmpSrtConnPublish:
				return "srt-publish";
			default:
				return "Unknown";
		}
	}

	bool srs_client_type_is_publish(RtmpRtmpConnType type)
	{
		return (type & 0xff00) == 0x0200;
	}

	RtmpClientInfo::RtmpClientInfo()
	{
		edge = false;
		req  = new RtmpRequest();
		res  = new RtmpResponse();
		type = RtmpRtmpConnUnknown;
	}

	RtmpClientInfo::~RtmpClientInfo()
	{
		FREEP(req);
		FREEP(res);
	}

} // namespace RTMP
