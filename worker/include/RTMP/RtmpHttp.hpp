#ifndef MS_RTMP_HTTP_HPP
#define MS_RTMP_HTTP_HPP

#include <map>
#include <stdint.h>
#include <string>
namespace RTMP
{

	// Used to resolve the http uri.
	class SrsHttpUri
	{
	private:
		std::string url_;
		std::string schema;
		std::string host;
		int port;
		std::string path;
		std::string query;
		std::string fragment_;
		std::string username_;
		std::string password_;
		std::map<std::string, std::string> query_values_;

	public:
		SrsHttpUri();
		virtual ~SrsHttpUri();

	public:
		// Initialize the http uri.
		virtual srs_error_t initialize(std::string _url);
		// After parsed the message, set the schema to https.
		virtual void set_schema(std::string v);

	public:
		virtual std::string get_url();
		virtual std::string get_schema();
		virtual std::string get_host();
		virtual int get_port();
		virtual std::string get_path();
		virtual std::string get_query();
		virtual std::string get_query_by_key(std::string key);
		virtual std::string get_fragment();
		virtual std::string username();
		virtual std::string password();

	private:
		// Get the parsed url field.
		// @return return empty string if not set.
		virtual std::string get_uri_field(const std::string& uri, void* hp_u, int field);
		srs_error_t parse_query();

		// public:
		// 	static std::string query_escape(std::string s);
		// 	static std::string path_escape(std::string s);
		// 	static srs_error_t query_unescape(std::string s, std::string& value);
		// 	static srs_error_t path_unescape(std::string s, std::string& value);
	};

	/**
	 * parse the tcUrl, output the schema, host, vhost, app and port.
	 * @param tcUrl, the input tcUrl, for example,
	 *       rtmp://192.168.1.10:19350/live?vhost=vhost.ossrs.net
	 * @param schema, for example, rtmp
	 * @param host, for example, 192.168.1.10
	 * @param vhost, for example, vhost.ossrs.net.
	 *       vhost default to host, when user not set vhost in query of app.
	 * @param app, for example, live
	 * @param port, for example, 19350
	 *       default to 1935 if not specified.
	 * param param, for example, vhost=vhost.ossrs.net
	 * @remark The param stream is input and output param, that is:
	 *       input: tcUrl+stream
	 *       output: schema, host, vhost, app, stream, port, param
	 */
	void srs_discovery_tc_url(
	  std::string tcUrl,
	  std::string& schema,
	  std::string& host,
	  std::string& vhost,
	  std::string& app,
	  std::string& stream,
	  int& port,
	  std::string& param);

	// Guessing stream by app and param, to make OBS happy. For example:
	//      rtmp://ip/live/livestream
	//      rtmp://ip/live/livestream?secret=xxx
	//      rtmp://ip/live?secret=xxx/livestream
	void srs_guess_stream_by_app(std::string& app, std::string& param, std::string& stream);

	// get the stream identify, vhost/app/stream.
	std::string srs_generate_stream_url(std::string vhost, std::string app, std::string stream);

	/**
	 * generate the tcUrl without param.
	 * @remark Use host as tcUrl.vhost if vhost is default vhost.
	 */
	std::string srs_generate_tc_url(
	  std::string schema, std::string host, std::string vhost, std::string app, int port);
} // namespace RTMP

#endif