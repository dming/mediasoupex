#ifndef MS_RTMP_RTMP_HPP
#define MS_RTMP_RTMP_HPP
#include "RTMP/RtmpCodec.hpp"
namespace RTMP
{
	class RtmpOnMetaDataPacket;
	class RtmpSharedPtrMessage;
	/**
	 * Create special structure from RTMP stream, for example, the metadata.
	 */
	class SrsRtmpFormat : public SrsFormat
	{
	public:
		SrsRtmpFormat();
		virtual ~SrsRtmpFormat();

	public:
		// Initialize the format from metadata, optional.
		virtual srs_error_t on_metadata(RtmpOnMetaDataPacket* meta);
		// When got a parsed audio packet.
		virtual srs_error_t on_audio(RtmpSharedPtrMessage* shared_audio);
		virtual srs_error_t on_audio(int64_t timestamp, char* data, int size);
		// When got a parsed video packet.
		virtual srs_error_t on_video(RtmpSharedPtrMessage* shared_video);
		virtual srs_error_t on_video(int64_t timestamp, char* data, int size);
	};
} // namespace RTMP

#endif