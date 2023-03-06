#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpUtility.hpp"
#include <stdarg.h>

namespace RTMP
{

	std::string srs_string_replace(std::string str, std::string old_str, std::string new_str)
	{
		std::string ret = str;

		if (old_str == new_str)
		{
			return ret;
		}

		size_t pos = 0;
		while ((pos = ret.find(old_str, pos)) != std::string::npos)
		{
			ret = ret.replace(pos, old_str.length(), new_str);
			pos += new_str.length();
		}

		return ret;
	}

	std::string srs_string_trim_end(std::string str, std::string trim_chars)
	{
		std::string ret = str;

		for (int i = 0; i < (int)trim_chars.length(); i++)
		{
			char ch = trim_chars.at(i);

			while (!ret.empty() && ret.at(ret.length() - 1) == ch)
			{
				ret.erase(ret.end() - 1);

				// ok, matched, should reset the search
				i = -1;
			}
		}

		return ret;
	}

	std::string srs_string_trim_start(std::string str, std::string trim_chars)
	{
		std::string ret = str;

		for (int i = 0; i < (int)trim_chars.length(); i++)
		{
			char ch = trim_chars.at(i);

			while (!ret.empty() && ret.at(0) == ch)
			{
				ret.erase(ret.begin());

				// ok, matched, should reset the search
				i = -1;
			}
		}

		return ret;
	}

	std::string srs_string_remove(std::string str, std::string remove_chars)
	{
		std::string ret = str;

		for (int i = 0; i < (int)remove_chars.length(); i++)
		{
			char ch = remove_chars.at(i);

			for (std::string::iterator it = ret.begin(); it != ret.end();)
			{
				if (ch == *it)
				{
					it = ret.erase(it);

					// ok, matched, should reset the search
					i = -1;
				}
				else
				{
					++it;
				}
			}
		}

		return ret;
	}

	std::string srs_erase_first_substr(std::string str, std::string erase_string)
	{
		std::string ret = str;

		size_t pos = ret.find(erase_string);

		if (pos != std::string::npos)
		{
			ret.erase(pos, erase_string.length());
		}

		return ret;
	}

	std::string srs_erase_last_substr(std::string str, std::string erase_string)
	{
		std::string ret = str;

		size_t pos = ret.rfind(erase_string);

		if (pos != std::string::npos)
		{
			ret.erase(pos, erase_string.length());
		}

		return ret;
	}

	bool srs_string_ends_with(std::string str, std::string flag)
	{
		const size_t pos = str.rfind(flag);
		return (pos != std::string::npos) && (pos == str.length() - flag.length());
	}

	bool srs_string_ends_with(std::string str, std::string flag0, std::string flag1)
	{
		return srs_string_ends_with(str, flag0) || srs_string_ends_with(str, flag1);
	}

	bool srs_string_ends_with(std::string str, std::string flag0, std::string flag1, std::string flag2)
	{
		return srs_string_ends_with(str, flag0) || srs_string_ends_with(str, flag1) ||
		       srs_string_ends_with(str, flag2);
	}

	bool srs_string_ends_with(
	  std::string str, std::string flag0, std::string flag1, std::string flag2, std::string flag3)
	{
		return srs_string_ends_with(str, flag0) || srs_string_ends_with(str, flag1) ||
		       srs_string_ends_with(str, flag2) || srs_string_ends_with(str, flag3);
	}

	bool srs_string_starts_with(std::string str, std::string flag)
	{
		return str.find(flag) == 0;
	}

	bool srs_string_starts_with(std::string str, std::string flag0, std::string flag1)
	{
		return srs_string_starts_with(str, flag0) || srs_string_starts_with(str, flag1);
	}

	bool srs_string_starts_with(std::string str, std::string flag0, std::string flag1, std::string flag2)
	{
		return srs_string_starts_with(str, flag0, flag1) || srs_string_starts_with(str, flag2);
	}

	bool srs_string_starts_with(
	  std::string str, std::string flag0, std::string flag1, std::string flag2, std::string flag3)
	{
		return srs_string_starts_with(str, flag0, flag1, flag2) || srs_string_starts_with(str, flag3);
	}

	bool srs_string_contains(std::string str, std::string flag)
	{
		return str.find(flag) != std::string::npos;
	}

	bool srs_string_contains(std::string str, std::string flag0, std::string flag1)
	{
		return str.find(flag0) != std::string::npos || str.find(flag1) != std::string::npos;
	}

	bool srs_string_contains(std::string str, std::string flag0, std::string flag1, std::string flag2)
	{
		return str.find(flag0) != std::string::npos || str.find(flag1) != std::string::npos ||
		       str.find(flag2) != std::string::npos;
	}

	int srs_string_count(std::string str, std::string flag)
	{
		int nn = 0;
		for (int i = 0; i < (int)flag.length(); i++)
		{
			char ch = flag.at(i);
			nn += std::count(str.begin(), str.end(), ch);
		}
		return nn;
	}

	std::vector<std::string> srs_string_split(std::string s, std::string seperator)
	{
		std::vector<std::string> result;
		if (seperator.empty())
		{
			result.push_back(s);
			return result;
		}

		size_t posBegin     = 0;
		size_t posSeperator = s.find(seperator);
		while (posSeperator != std::string::npos)
		{
			result.push_back(s.substr(posBegin, posSeperator - posBegin));
			posBegin     = posSeperator + seperator.length(); // next byte of seperator
			posSeperator = s.find(seperator, posBegin);
		}
		// push the last element
		result.push_back(s.substr(posBegin));
		return result;
	}

	std::string srs_string_min_match(std::string str, std::vector<std::string> seperators)
	{
		std::string match;

		if (seperators.empty())
		{
			return str;
		}

		size_t min_pos = std::string::npos;
		for (std::vector<std::string>::iterator it = seperators.begin(); it != seperators.end(); ++it)
		{
			std::string seperator = *it;

			size_t pos = str.find(seperator);
			if (pos == std::string::npos)
			{
				continue;
			}

			if (min_pos == std::string::npos || pos < min_pos)
			{
				min_pos = pos;
				match   = seperator;
			}
		}

		return match;
	}

	std::vector<std::string> srs_string_split(std::string str, std::vector<std::string> seperators)
	{
		std::vector<std::string> arr;

		size_t pos    = std::string::npos;
		std::string s = str;

		while (true)
		{
			std::string seperator = srs_string_min_match(s, seperators);
			if (seperator.empty())
			{
				break;
			}

			if ((pos = s.find(seperator)) == std::string::npos)
			{
				break;
			}

			arr.push_back(s.substr(0, pos));
			s = s.substr(pos + seperator.length());
		}

		if (!s.empty())
		{
			arr.push_back(s);
		}

		return arr;
	}

	std::string srs_fmt(const char* fmt, ...)
	{
		va_list ap;
		va_start(ap, fmt);

		static char buf[8192];
		int r0 = vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);

		std::string v;
		if (r0 > 0 && r0 < (int)sizeof(buf))
		{
			v.append(buf, r0);
		}

		return v;
	}

	bool srs_path_exists(std::string path)
	{
		struct stat st;

		// stat current dir, if exists, return error.
		if (stat(path.c_str(), &st) == 0)
		{
			return true;
		}

		return false;
	}

	std::string srs_path_dirname(std::string path)
	{
		std::string dirname = path;

		// No slash, it must be current dir.
		size_t pos = std::string::npos;
		if ((pos = dirname.rfind("/")) == std::string::npos)
		{
			return "./";
		}

		// Path under root.
		if (pos == 0)
		{
			return "/";
		}

		// Fetch the directory.
		dirname = dirname.substr(0, pos);
		return dirname;
	}

	std::string srs_path_basename(std::string path)
	{
		std::string dirname = path;
		size_t pos          = std::string::npos;

		if ((pos = dirname.rfind("/")) != std::string::npos)
		{
			// the basename("/") is "/"
			if (dirname.length() == 1)
			{
				return dirname;
			}
			dirname = dirname.substr(pos + 1);
		}

		return dirname;
	}

	std::string srs_path_filename(std::string path)
	{
		std::string filename = path;
		size_t pos           = std::string::npos;

		if ((pos = filename.rfind(".")) != std::string::npos)
		{
			return filename.substr(0, pos);
		}

		return filename;
	}

	std::string srs_path_filext(std::string path)
	{
		size_t pos = std::string::npos;

		if ((pos = path.rfind(".")) != std::string::npos)
		{
			return path.substr(pos);
		}

		return "";
	}

	void srs_parse_query_string(std::string q, std::map<std::string, std::string>& query)
	{
		// query std::string flags.
		static std::vector<std::string> flags;
		if (flags.empty())
		{
			flags.push_back("=");
			flags.push_back(",");
			flags.push_back("&&");
			flags.push_back("&");
			flags.push_back(";");
		}

		std::vector<std::string> kvs = srs_string_split(q, flags);
		for (int i = 0; i < (int)kvs.size(); i += 2)
		{
			std::string k = kvs.at(i);
			std::string v = (i < (int)kvs.size() - 1) ? kvs.at(i + 1) : "";

			query[k] = v;
		}
	}

	int srs_chunk_header_c0(
	  int perfer_cid,
	  uint32_t timestamp,
	  int32_t payload_length,
	  int8_t message_type,
	  int32_t stream_id,
	  char* cache,
	  int nb_cache)
	{
		// to directly set the field.
		char* pp = NULL;

		// generate the header.
		char* p = cache;

		// no header.
		if (nb_cache < SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE)
		{
			return 0;
		}

		// write new chunk stream header, fmt is 0
		*p++ = 0x00 | (perfer_cid & 0x3F);

		// chunk message header, 11 bytes
		// timestamp, 3bytes, big-endian
		if (timestamp < RTMP_EXTENDED_TIMESTAMP)
		{
			pp   = (char*)&timestamp;
			*p++ = pp[2];
			*p++ = pp[1];
			*p++ = pp[0];
		}
		else
		{
			*p++ = (char)0xFF;
			*p++ = (char)0xFF;
			*p++ = (char)0xFF;
		}

		// message_length, 3bytes, big-endian
		pp   = (char*)&payload_length;
		*p++ = pp[2];
		*p++ = pp[1];
		*p++ = pp[0];

		// message_type, 1bytes
		*p++ = message_type;

		// stream_id, 4bytes, little-endian
		pp   = (char*)&stream_id;
		*p++ = pp[0];
		*p++ = pp[1];
		*p++ = pp[2];
		*p++ = pp[3];

		// for c0
		// chunk extended timestamp header, 0 or 4 bytes, big-endian
		//
		// for c3:
		// chunk extended timestamp header, 0 or 4 bytes, big-endian
		// 6.1.3. Extended Timestamp
		// This field is transmitted only when the normal time stamp in the
		// chunk message header is set to 0x00ffffff. If normal time stamp is
		// set to any value less than 0x00ffffff, this field MUST NOT be
		// present. This field MUST NOT be present if the timestamp field is not
		// present. Type 3 chunks MUST NOT have this field.
		// adobe changed for Type3 chunk:
		//        FMLE always sendout the extended-timestamp,
		//        must send the extended-timestamp to FMS,
		//        must send the extended-timestamp to flash-player.
		// @see: ngx_rtmp_prepare_message
		// @see: http://blog.csdn.net/win_lin/article/details/13363699
		// TODO: FIXME: extract to outer.
		if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
		{
			pp   = (char*)&timestamp;
			*p++ = pp[3];
			*p++ = pp[2];
			*p++ = pp[1];
			*p++ = pp[0];
		}

		// always has header
		return (int)(p - cache);
	}

	int srs_chunk_header_c3(int perfer_cid, uint32_t timestamp, char* cache, int nb_cache)
	{
		// to directly set the field.
		char* pp = NULL;

		// generate the header.
		char* p = cache;

		// no header.
		if (nb_cache < SRS_CONSTS_RTMP_MAX_FMT3_HEADER_SIZE)
		{
			return 0;
		}

		// write no message header chunk stream, fmt is 3
		// @remark, if perfer_cid > 0x3F, that is, use 2B/3B chunk header,
		// SRS will rollback to 1B chunk header.
		*p++ = 0xC0 | (perfer_cid & 0x3F);

		// for c0
		// chunk extended timestamp header, 0 or 4 bytes, big-endian
		//
		// for c3:
		// chunk extended timestamp header, 0 or 4 bytes, big-endian
		// 6.1.3. Extended Timestamp
		// This field is transmitted only when the normal time stamp in the
		// chunk message header is set to 0x00ffffff. If normal time stamp is
		// set to any value less than 0x00ffffff, this field MUST NOT be
		// present. This field MUST NOT be present if the timestamp field is not
		// present. Type 3 chunks MUST NOT have this field.
		// adobe changed for Type3 chunk:
		//        FMLE always sendout the extended-timestamp,
		//        must send the extended-timestamp to FMS,
		//        must send the extended-timestamp to flash-player.
		// @see: ngx_rtmp_prepare_message
		// @see: http://blog.csdn.net/win_lin/article/details/13363699
		// TODO: FIXME: extract to outer.
		if (timestamp >= RTMP_EXTENDED_TIMESTAMP)
		{
			pp   = (char*)&timestamp;
			*p++ = pp[3];
			*p++ = pp[2];
			*p++ = pp[1];
			*p++ = pp[0];
		}

		// always has header
		return (int)(p - cache);
	}

} // namespace RTMP
