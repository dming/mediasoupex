
#ifndef MS_RTMP_JITTER_HPP
#define MS_RTMP_JITTER_HPP

#include "RTMP/RtmpMessage.hpp"
#include <stdint.h>
#include <string>

namespace RTMP
{

	// The time jitter algorithm:
	// 1. full, to ensure stream start at zero, and ensure stream monotonically increasing.
	// 2. zero, only ensure sttream start at zero, ignore timestamp jitter.
	// 3. off, disable the time jitter algorithm, like atc.
	enum RtmpRtmpJitterAlgorithm
	{
		RtmpRtmpJitterAlgorithmFULL = 0x01,
		RtmpRtmpJitterAlgorithmZERO,
		RtmpRtmpJitterAlgorithmOFF
	};
	int srs_time_jitter_string2int(std::string time_jitter);

	// Time jitter detect and correct, to ensure the rtmp stream is monotonically.
	class RtmpRtmpJitter
	{
	public:
		RtmpRtmpJitter();
		virtual ~RtmpRtmpJitter();

	public:
		// detect the time jitter and correct it.
		// @param ag the algorithm to use for time jitter.
		virtual srs_error_t correct(RtmpSharedPtrMessage* msg, RtmpRtmpJitterAlgorithm ag);
		// Get current client time, the last packet time.
		virtual int64_t get_time();

	private:
		int64_t last_pkt_time;
		int64_t last_pkt_correct_time;
	};
} // namespace RTMP

#endif