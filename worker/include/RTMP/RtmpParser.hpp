#ifndef MS_RTMP_PARSER_HPP
#define MS_RTMP_PARSER_HPP

#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPacket.hpp"
#include <map>
#include <stdint.h>
#include <string>

namespace RTMP
{
	class RtmpParser
	{
	public:
		RtmpParser()          = default;
		virtual ~RtmpParser() = default;

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

	private:
		// The requests sent out, used to build the response.
		// key: transactionId
		// value: the request command name
		std::map<double, std::string> requests;
	};
} // namespace RTMP

#endif