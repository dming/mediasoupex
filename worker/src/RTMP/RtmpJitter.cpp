#define MS_CLASS "RTMP::RtmpJitter"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpJitter.hpp"

#define CONST_MAX_JITTER_MS 250
#define CONST_MAX_JITTER_MS_NEG -250
#define DEFAULT_FRAME_TIME_MS 10

namespace RTMP
{

	int srs_time_jitter_string2int(std::string time_jitter)
	{
		if (time_jitter == "full")
		{
			return RtmpRtmpJitterAlgorithmFULL;
		}
		else if (time_jitter == "zero")
		{
			return RtmpRtmpJitterAlgorithmZERO;
		}
		else
		{
			return RtmpRtmpJitterAlgorithmOFF;
		}
	}

	RtmpRtmpJitter::RtmpRtmpJitter()
	{
		last_pkt_correct_time = -1;
		last_pkt_time         = 0;
	}

	RtmpRtmpJitter::~RtmpRtmpJitter()
	{
	}

	srs_error_t RtmpRtmpJitter::correct(RtmpSharedPtrMessage* msg, RtmpRtmpJitterAlgorithm ag)
	{
		srs_error_t err = srs_success;

		// for performance issue
		if (ag != RtmpRtmpJitterAlgorithmFULL)
		{
			// all jitter correct features is disabled, ignore.
			if (ag == RtmpRtmpJitterAlgorithmOFF)
			{
				return err;
			}

			// start at zero, but donot ensure monotonically increasing.
			if (ag == RtmpRtmpJitterAlgorithmZERO)
			{
				// for the first time, last_pkt_correct_time is -1.
				if (last_pkt_correct_time == -1)
				{
					last_pkt_correct_time = msg->timestamp;
				}
				msg->timestamp -= last_pkt_correct_time;
				return err;
			}

			// other algorithm, ignore.
			return err;
		}

		// full jitter algorithm, do jitter correct.
		// set to 0 for metadata.
		if (!msg->is_av())
		{
			msg->timestamp = 0;
			return err;
		}

		/**
		 * we use a very simple time jitter detect/correct algorithm:
		 * 1. delta: ensure the delta is positive and valid,
		 *     we set the delta to DEFAULT_FRAME_TIME_MS,
		 *     if the delta of time is nagative or greater than CONST_MAX_JITTER_MS.
		 * 2. last_pkt_time: specifies the original packet time,
		 *     is used to detect next jitter.
		 * 3. last_pkt_correct_time: simply add the positive delta,
		 *     and enforce the time monotonically.
		 */
		int64_t time  = msg->timestamp;
		int64_t delta = time - last_pkt_time;

		// if jitter detected, reset the delta.
		if (delta < CONST_MAX_JITTER_MS_NEG || delta > CONST_MAX_JITTER_MS)
		{
			// use default 10ms to notice the problem of stream.
			// @see https://github.com/ossrs/srs/issues/425
			delta = DEFAULT_FRAME_TIME_MS;
		}

		last_pkt_correct_time = std::max((int64_t)0, last_pkt_correct_time + delta);

		msg->timestamp = last_pkt_correct_time;
		last_pkt_time  = time;

		return err;
	}

	int64_t RtmpRtmpJitter::get_time()
	{
		return last_pkt_correct_time;
	}
} // namespace RTMP
