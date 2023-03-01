#ifndef MS_RTMP_UTILITY_HPP
#define MS_RTMP_UTILITY_HPP

#include "RTMP/RtmpKernel.hpp"
#include <map>
#include <string>
#include <vector>

namespace RTMP
{

	// Replace old_str to new_str of str
	std::string srs_string_replace(std::string str, std::string old_str, std::string new_str);
	// Trim char in trim_chars of str
	std::string srs_string_trim_end(std::string str, std::string trim_chars);
	// Trim char in trim_chars of str
	std::string srs_string_trim_start(std::string str, std::string trim_chars);
	// Remove char in remove_chars of str
	std::string srs_string_remove(std::string str, std::string remove_chars);
	// Remove first substring from str
	std::string srs_erase_first_substr(std::string str, std::string erase_string);
	// Remove last substring from str
	std::string srs_erase_last_substr(std::string str, std::string erase_string);
	// Whether string end with
	bool srs_string_ends_with(std::string str, std::string flag);
	bool srs_string_ends_with(std::string str, std::string flag0, std::string flag1);
	bool srs_string_ends_with(std::string str, std::string flag0, std::string flag1, std::string flag2);
	bool srs_string_ends_with(
	  std::string str, std::string flag0, std::string flag1, std::string flag2, std::string flag3);
	// Whether string starts with
	bool srs_string_starts_with(std::string str, std::string flag);
	bool srs_string_starts_with(std::string str, std::string flag0, std::string flag1);
	bool srs_string_starts_with(std::string str, std::string flag0, std::string flag1, std::string flag2);
	bool srs_string_starts_with(
	  std::string str, std::string flag0, std::string flag1, std::string flag2, std::string flag3);
	// Whether string contains with
	bool srs_string_contains(std::string str, std::string flag);
	bool srs_string_contains(std::string str, std::string flag0, std::string flag1);
	bool srs_string_contains(std::string str, std::string flag0, std::string flag1, std::string flag2);
	// Count each char of flag in string
	int srs_string_count(std::string str, std::string flag);
	// Find the min match in str for flags.
	std::string srs_string_min_match(std::string str, std::vector<std::string> flags);
	// Split the string by seperator to array.
	std::vector<std::string> srs_string_split(std::string s, std::string seperator);
	std::vector<std::string> srs_string_split(std::string s, std::vector<std::string> seperators);
	// Format to a string.
	std::string srs_fmt(const char* fmt, ...);

	// Whether path exists.
	bool srs_path_exists(std::string path);
	// Get the dirname of path, for instance, dirname("/live/livestream")="/live"
	std::string srs_path_dirname(std::string path);
	// Get the basename of path, for instance, basename("/live/livestream")="livestream"
	std::string srs_path_basename(std::string path);
	// Get the filename of path, for instance, filename("livestream.flv")="livestream"
	std::string srs_path_filename(std::string path);
	// Get the file extension of path, for instance, filext("live.flv")=".flv"
	std::string srs_path_filext(std::string path);

	// parse query string to map(k,v).
	// must format as key=value&...&keyN=valueN
	void srs_parse_query_string(std::string q, std::map<std::string, std::string>& query);

} // namespace RTMP

#endif