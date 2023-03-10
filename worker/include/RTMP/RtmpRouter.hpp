#ifndef MS_RTMP_ROUTER_HPP
#define MS_RTMP_ROUTER_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpCodec.hpp"
#include "RTMP/RtmpConsumer.hpp"
#include "RTMP/RtmpFormat.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpPublisher.hpp"
#include <map>
#include <stdint.h>
#include <unordered_map>

namespace RTMP
{
	class RtmpSession;
	class RtmpRequest;

	// The time jitter algorithm:
	// 1. full, to ensure stream start at zero, and ensure stream monotonically increasing.
	// 2. zero, only ensure sttream start at zero, ignore timestamp jitter.
	// 3. off, disable the time jitter algorithm, like atc.
	enum SrsRtmpJitterAlgorithm
	{
		SrsRtmpJitterAlgorithmFULL = 0x01,
		SrsRtmpJitterAlgorithmZERO,
		SrsRtmpJitterAlgorithmOFF
	};
	int srs_time_jitter_string2int(std::string time_jitter);

	// Time jitter detect and correct, to ensure the rtmp stream is monotonically.
	class SrsRtmpJitter
	{
	private:
		int64_t last_pkt_time;
		int64_t last_pkt_correct_time;

	public:
		SrsRtmpJitter();
		virtual ~SrsRtmpJitter();

	public:
		// detect the time jitter and correct it.
		// @param ag the algorithm to use for time jitter.
		virtual srs_error_t correct(RtmpSharedPtrMessage* msg, SrsRtmpJitterAlgorithm ag);
		// Get current client time, the last packet time.
		virtual int64_t get_time();
	};

	// The mix queue to correct the timestamp for mix_correct algorithm.
	class RtmpMixQueue
	{
	private:
		uint32_t nb_videos;
		uint32_t nb_audios;
		std::multimap<int64_t, RtmpSharedPtrMessage*> msgs;

	public:
		RtmpMixQueue();
		virtual ~RtmpMixQueue();

	public:
		virtual void clear();
		virtual void push(RtmpSharedPtrMessage* msg);
		virtual RtmpSharedPtrMessage* pop();
	};

	// Each stream have optional meta(sps/pps in sequence header and metadata).
	// This class cache and update the meta.
	class SrsMetaCache
	{
	private:
		// The cached metadata, FLV script data tag.
		RtmpSharedPtrMessage* meta;
		// The cached video sequence header, for example, sps/pps for h.264.
		RtmpSharedPtrMessage* video;
		RtmpSharedPtrMessage* previous_video;
		// The cached audio sequence header, for example, asc for aac.
		RtmpSharedPtrMessage* audio;
		RtmpSharedPtrMessage* previous_audio;
		// The format for sequence header.
		SrsRtmpFormat* vformat;
		SrsRtmpFormat* aformat;

	public:
		SrsMetaCache();
		virtual ~SrsMetaCache();

	public:
		// Dispose the metadata cache.
		virtual void dispose();
		// For each publishing, clear the metadata cache.
		virtual void clear();

	public:
		// Get the cached metadata.
		virtual RtmpSharedPtrMessage* data();
		// Get the cached vsh(video sequence header).
		virtual RtmpSharedPtrMessage* vsh();
		virtual SrsFormat* vsh_format();
		// Get the cached ash(audio sequence header).
		virtual RtmpSharedPtrMessage* ash();
		virtual SrsFormat* ash_format();
		// // Dumps cached metadata to consumer.
		// // @param dm Whether dumps the metadata.
		// // @param ds Whether dumps the sequence header.
		virtual srs_error_t dumps(
		  RtmpConsumer* consumer, bool atc, int streamId, SrsRtmpJitterAlgorithm ag, bool dm, bool ds);

	public:
		// Previous exists sequence header.
		virtual RtmpSharedPtrMessage* previous_vsh();
		virtual RtmpSharedPtrMessage* previous_ash();
		// Update previous sequence header, drop old one, set to new sequence header.
		virtual void update_previous_vsh();
		virtual void update_previous_ash();

	public:
		// Update the cached metadata by packet.
		virtual srs_error_t update_data(
		  RtmpMessageHeader* header, RtmpOnMetaDataPacket* metadata, bool& updated);
		// Update the cached audio sequence header.
		virtual srs_error_t update_ash(RtmpSharedPtrMessage* msg);
		// Update the cached video sequence header.
		virtual srs_error_t update_vsh(RtmpSharedPtrMessage* msg);
	};

	class RtmpRouter
	{
	public:
		RtmpRouter(/* args */);
		~RtmpRouter();

		srs_error_t initualize(RtmpRequest* req);
		srs_error_t CreatePublisher(RtmpSession* session, RtmpPublisher** publisher);
		srs_error_t RemoveSession(RtmpSession* session);

	private:
		// The time jitter algorithm for vhost.
		SrsRtmpJitterAlgorithm jitter_algorithm;
		// whether stream is monotonically increase.
		bool is_monotonically_increase;
		// The time of the packet we just got.
		int64_t last_packet_time;
		// For play, whether use interlaced/mixed algorithm to correct timestamp.
		bool mix_correct;
		// The mix queue to implements the mix correct algorithm.
		RtmpMixQueue* mix_queue;
		// For play, whether enabled atc.
		// The atc(use absolute time and donot adjust time),
		// directly use msg time and donot adjust if atc is true,
		// otherwise, adjust msg time to start from 0 to make flash happy.
		bool atc;
		// The metadata cache.
		SrsMetaCache* meta;
		// The format, codec information.
		SrsRtmpFormat* format_;

	public:
		srs_error_t CreateConsumer(RtmpSession* session, RtmpConsumer*& consumer);
		srs_error_t ConsumerDump(RtmpConsumer* consumer);

		virtual srs_error_t on_meta_data(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata);

		srs_error_t OnAudio(RtmpCommonMessage* shared_audio);
		srs_error_t OnVideo(RtmpCommonMessage* shared_video);

	private:
		srs_error_t on_audio_imp(RtmpSharedPtrMessage* msg);
		srs_error_t on_video_imp(RtmpSharedPtrMessage* msg);

	private:
		RtmpRequest* req_;
		RtmpPublisher* publisher_;
		std::unordered_map<uint64_t, RtmpConsumer*> consumers_;
	};

} // namespace RTMP

#endif