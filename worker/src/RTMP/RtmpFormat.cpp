#define MS_CLASS "RTMP::RtmpFormat"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpFormat.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpPacket.hpp"
namespace RTMP
{

	SrsRtmpFormat::SrsRtmpFormat()
	{
	}

	SrsRtmpFormat::~SrsRtmpFormat()
	{
	}

	srs_error_t SrsRtmpFormat::on_metadata(RtmpOnMetaDataPacket* meta)
	{
		// TODO: FIXME: Try to initialize format from metadata.
		return srs_success;
	}

	srs_error_t SrsRtmpFormat::on_audio(RtmpSharedPtrMessage* shared_audio)
	{
		RtmpSharedPtrMessage* msg = shared_audio;
		char* data                = msg->payload;
		int size                  = msg->size;

		return SrsFormat::on_audio(msg->timestamp, data, size);
	}

	srs_error_t SrsRtmpFormat::on_audio(int64_t timestamp, char* data, int size)
	{
		return SrsFormat::on_audio(timestamp, data, size);
	}

	srs_error_t SrsRtmpFormat::on_video(RtmpSharedPtrMessage* shared_video)
	{
		RtmpSharedPtrMessage* msg = shared_video;
		char* data                = msg->payload;
		int size                  = msg->size;

		return SrsFormat::on_video(msg->timestamp, data, size);
	}

	srs_error_t SrsRtmpFormat::on_video(int64_t timestamp, char* data, int size)
	{
		return SrsFormat::on_video(timestamp, data, size);
	}

} // namespace RTMP
