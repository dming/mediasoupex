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
} // namespace RTMP
