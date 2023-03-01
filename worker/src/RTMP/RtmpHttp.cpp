#define MS_CLASS "RTMP::RtmpHttp"
#define MS_LOG_DEV_LEVEL 3

extern "C"
{
#include <http_parser.h>
}
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpUtility.hpp"

namespace RTMP
{
	SrsHttpUri::SrsHttpUri()
	{
		port = 0;
	}

	SrsHttpUri::~SrsHttpUri()
	{
	}

	srs_error_t SrsHttpUri::initialize(std::string url)
	{
		schema = host = path = query = fragment_ = "";
		url_                                     = url;

		// Replace the default vhost to a domain like std::string, or parse failed.
		std::string parsing_url  = url;
		size_t pos_default_vhost = url.find("://__defaultVhost__");
		if (pos_default_vhost != std::string::npos)
		{
			parsing_url =
			  srs_string_replace(parsing_url, "://__defaultVhost__", "://safe.vhost.default.ossrs.io");
		}

		http_parser_url hp_u;
		http_parser_url_init(&hp_u);

		int r0;
		if ((r0 = http_parser_parse_url(parsing_url.c_str(), parsing_url.length(), 0, &hp_u)) != 0)
		{
			return srs_error_new(
			  ERROR_HTTP_PARSE_URI, "parse url %s as %s failed, code=%d", url.c_str(), parsing_url.c_str(), r0);
		}

		std::string field = get_uri_field(parsing_url, &hp_u, UF_SCHEMA);
		if (!field.empty())
		{
			schema = field;
		}

		// Restore the default vhost.
		if (pos_default_vhost == std::string::npos)
		{
			host = get_uri_field(parsing_url, &hp_u, UF_HOST);
		}
		else
		{
			host = SRS_CONSTS_RTMP_DEFAULT_VHOST;
		}

		field = get_uri_field(parsing_url, &hp_u, UF_PORT);
		if (!field.empty())
		{
			port = ::atoi(field.c_str());
		}
		if (port <= 0)
		{
			if (schema == "https")
			{
				port = SRS_DEFAULT_HTTPS_PORT;
			}
			else if (schema == "rtmp")
			{
				port = SRS_CONSTS_RTMP_DEFAULT_PORT;
			}
			else if (schema == "redis")
			{
				port = SRS_DEFAULT_REDIS_PORT;
			}
			else
			{
				port = SRS_DEFAULT_HTTP_PORT;
			}
		}

		path      = get_uri_field(parsing_url, &hp_u, UF_PATH);
		query     = get_uri_field(parsing_url, &hp_u, UF_QUERY);
		fragment_ = get_uri_field(parsing_url, &hp_u, UF_FRAGMENT);

		username_  = get_uri_field(parsing_url, &hp_u, UF_USERINFO);
		size_t pos = username_.find(":");
		if (pos != std::string::npos)
		{
			password_ = username_.substr(pos + 1);
			username_ = username_.substr(0, pos);
		}

		return parse_query();
	}

	void SrsHttpUri::set_schema(std::string v)
	{
		schema = v;

		// Update url with new schema.
		size_t pos = url_.find("://");
		if (pos != std::string::npos)
		{
			url_ = schema + "://" + url_.substr(pos + 3);
		}
	}

	std::string SrsHttpUri::get_url()
	{
		return url_;
	}

	std::string SrsHttpUri::get_schema()
	{
		return schema;
	}

	std::string SrsHttpUri::get_host()
	{
		return host;
	}

	int SrsHttpUri::get_port()
	{
		return port;
	}

	std::string SrsHttpUri::get_path()
	{
		return path;
	}

	std::string SrsHttpUri::get_query()
	{
		return query;
	}

	std::string SrsHttpUri::get_query_by_key(std::string key)
	{
		std::map<std::string, std::string>::iterator it = query_values_.find(key);
		if (it == query_values_.end())
		{
			return "";
		}
		return it->second;
	}

	std::string SrsHttpUri::get_fragment()
	{
		return fragment_;
	}

	std::string SrsHttpUri::username()
	{
		return username_;
	}

	std::string SrsHttpUri::password()
	{
		return password_;
	}

	std::string SrsHttpUri::get_uri_field(const std::string& uri, void* php_u, int ifield)
	{
		http_parser_url* hp_u        = (http_parser_url*)php_u;
		http_parser_url_fields field = (http_parser_url_fields)ifield;

		if ((hp_u->field_set & (1 << field)) == 0)
		{
			return "";
		}

		int offset = hp_u->field_data[field].off;
		int len    = hp_u->field_data[field].len;

		return uri.substr(offset, len);
	}

	srs_error_t SrsHttpUri::parse_query()
	{
		srs_error_t err = srs_success;
		if (query.empty())
		{
			return err;
		}

		size_t begin = query.find("?");
		if (std::string::npos != begin)
		{
			begin++;
		}
		else
		{
			begin = 0;
		}
		std::string query_str = query.substr(begin);
		query_values_.clear();
		srs_parse_query_string(query_str, query_values_);

		return err;
	}

	void srs_discovery_tc_url(
	  std::string tcUrl,
	  std::string& schema,
	  std::string& host,
	  std::string& vhost,
	  std::string& app,
	  std::string& stream,
	  int& port,
	  std::string& param)
	{
		// Standard URL is:
		//      rtmp://ip/app/app2/stream?k=v
		// Where after last slash is stream.
		std::string fullUrl = tcUrl;
		fullUrl += stream.empty() ? "/" : (stream.at(0) == '/' ? stream : "/" + stream);
		fullUrl += param.empty() ? "" : (param.at(0) == '?' ? param : "?" + param);

		// First, we covert the FMLE URL to standard URL:
		//      rtmp://ip/app/app2?k=v/stream , or:
		//      rtmp://ip/app/app2#k=v/stream
		size_t pos_query  = fullUrl.find_first_of("?#");
		size_t pos_rslash = fullUrl.rfind("/");
		if (pos_rslash != std::string::npos && pos_query != std::string::npos && pos_query < pos_rslash)
		{
			fullUrl = fullUrl.substr(0, pos_query)                         // rtmp://ip/app/app2
			          + fullUrl.substr(pos_rslash)                         // /stream
			          + fullUrl.substr(pos_query, pos_rslash - pos_query); // ?k=v
		}

		// Remove the _definst_ of FMLE URL.
		if (fullUrl.find("/_definst_") != std::string::npos)
		{
			fullUrl = srs_string_replace(fullUrl, "/_definst_", "");
		}

		// Parse the standard URL.
		SrsHttpUri uri;
		srs_error_t err = srs_success;
		if ((err = uri.initialize(fullUrl)) != srs_success)
		{
			MS_WARN_DEV("Ignore parse url=%s err %s", fullUrl.c_str(), srs_error_desc(err).c_str());
			FREEP(err);
			return;
		}

		schema = uri.get_schema();
		host   = uri.get_host();
		port   = uri.get_port();
		stream = srs_path_basename(uri.get_path());
		param  = uri.get_query().empty() ? "" : "?" + uri.get_query();
		param += uri.get_fragment().empty() ? "" : "#" + uri.get_fragment();

		// Parse app without the prefix slash.
		app = srs_path_dirname(uri.get_path());
		if (!app.empty() && app.at(0) == '/')
			app = app.substr(1);
		if (app.empty())
			app = SRS_CONSTS_RTMP_DEFAULT_APP;

		// Try to parse vhost from query, or use host if not specified.
		std::string vhost_in_query = uri.get_query_by_key("vhost");
		if (vhost_in_query.empty())
			vhost_in_query = uri.get_query_by_key("domain");
		if (!vhost_in_query.empty() && vhost_in_query != SRS_CONSTS_RTMP_DEFAULT_VHOST)
			vhost = vhost_in_query;
		if (vhost.empty())
			vhost = host;

		// Only one param, the default vhost, clear it.
		if (param.find("&") == std::string::npos && vhost_in_query == SRS_CONSTS_RTMP_DEFAULT_VHOST)
		{
			param = "";
		}
	}

	void srs_guess_stream_by_app(std::string& app, std::string& param, std::string& stream)
	{
		size_t pos = std::string::npos;

		// Extract stream from app, if contains slash.
		if ((pos = app.find("/")) != std::string::npos)
		{
			stream = app.substr(pos + 1);
			app    = app.substr(0, pos);

			if ((pos = stream.find("?")) != std::string::npos)
			{
				param  = stream.substr(pos);
				stream = stream.substr(0, pos);
			}
			return;
		}

		// Extract stream from param, if contains slash.
		if ((pos = param.find("/")) != std::string::npos)
		{
			stream = param.substr(pos + 1);
			param  = param.substr(0, pos);
		}
	}

	std::string srs_generate_stream_url(std::string vhost, std::string app, std::string stream)
	{
		std::string url = "";

		if (SRS_CONSTS_RTMP_DEFAULT_VHOST != vhost)
		{
			url += vhost;
		}
		url += "/" + app;
		// Note that we ignore any extension.
		url += "/" + srs_path_filename(stream);

		return url;
	}

	std::string srs_generate_tc_url(
	  std::string schema, std::string host, std::string vhost, std::string app, int port)
	{
		std::string tcUrl = schema + "://";

		if (vhost == SRS_CONSTS_RTMP_DEFAULT_VHOST)
		{
			tcUrl += host.empty() ? SRS_CONSTS_RTMP_DEFAULT_VHOST : host;
		}
		else
		{
			tcUrl += vhost;
		}

		if (port && port != SRS_CONSTS_RTMP_DEFAULT_PORT)
		{
			tcUrl += ":" + std::to_string(port);
		}

		tcUrl += "/" + app;

		return tcUrl;
	}
} // namespace RTMP
