#ifndef MS_RTMP_ROUTER_HPP
#define MS_RTMP_ROUTER_HPP

#include "CplxError.hpp"
#include "RTMP/RtmpCodec.hpp"
#include "RTMP/RtmpFormat.hpp"
#include "RTMP/RtmpInfo.hpp"
#include "RTMP/RtmpKernel.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "RTMP/RtmpServerTransport.hpp"
#include "RTC/Shared.hpp"
#include <absl/container/flat_hash_map.h>
#include <nlohmann/json.hpp>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <vector>

namespace RTMP
{
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
	class RtmpMetaCache
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
		RtmpRtmpFormat* vformat;
		RtmpRtmpFormat* aformat;

	public:
		RtmpMetaCache();
		virtual ~RtmpMetaCache();

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
		  RtmpConsumer* consumer, bool atc, RtmpRtmpJitterAlgorithm ag, bool dm, bool ds);

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

	// cache a gop of video/audio data,
	// delivery at the connect of flash player,
	// To enable it to fast startup.
	class RtmpGopCache
	{
	private:
		// if disabled the gop cache,
		// The client will wait for the next keyframe for h264,
		// and will be black-screen.
		bool enable_gop_cache;
		// to limit the max gop cache frames
		// without this limit, if ingest stream always has no IDR frame
		// it will cause srs run out of memory
		int gop_cache_max_frames_;
		// The video frame count, avoid cache for pure audio stream.
		int cached_video_count;
		// when user disabled video when publishing, and gop cache enalbed,
		// We will cache the audio/video for we already got video, but we never
		// know when to clear the gop cache, for there is no video in future,
		// so we must guess whether user disabled the video.
		// when we got some audios after laster video, for instance, 600 audio packets,
		// about 3s(26ms per packet) 115 audio packets, clear gop cache.
		//
		// @remark, it is ok for performance, for when we clear the gop cache,
		//       gop cache is disabled for pure audio stream.
		// @see: https://github.com/ossrs/srs/issues/124
		int audio_after_last_video_count;
		// cached gop.
		std::vector<RtmpSharedPtrMessage*> gop_cache;

	public:
		RtmpGopCache();
		virtual ~RtmpGopCache();

	public:
		// cleanup when system quit.
		virtual void dispose();
		// To enable or disable the gop cache.
		virtual void set(bool v);
		virtual void set_gop_cache_max_frames(int v);
		virtual bool enabled();
		// only for h264 codec
		// 1. cache the gop when got h264 video packet.
		// 2. clear gop when got keyframe.
		// @param shared_msg, directly ptr, copy it if need to save it.
		virtual srs_error_t cache(RtmpSharedPtrMessage* shared_msg);
		// clear the gop cache.
		virtual void clear();
		// dump the cached gop to consumer.
		virtual srs_error_t dump(RtmpConsumer* consumer, bool atc, RtmpRtmpJitterAlgorithm jitter_algorithm);
		// used for atc to get the time of gop cache,
		// The atc will adjust the sequence header timestamp to gop cache.
		virtual bool empty();
		// Get the start time of gop cache, in srs_utime_t.
		// @return 0 if no packets.
		virtual srs_utime_t start_time();
		// whether current stream is pure audio,
		// when no video in gop cache, the stream is pure audio right now.
		virtual bool pure_audio();
	};

	class RtmpRouter : public Channel::ChannelSocket::RequestHandler, public RtmpTransport::Listener
	{
	public:
		RtmpRouter(RTC::Shared* shared, std::string id);
		~RtmpRouter();

	public:
		srs_error_t initualize(RtmpRequest* req);
		srs_error_t CreateServerTransport(
		  RtmpServerSession* session, bool isPublisher, RtmpServerTransport** transport);
		srs_error_t RemoveServerSession(RtmpServerSession* session);

		/* Methods inherited from Channel::ChannelSocket::RequestHandler. */
	public:
		void HandleRequest(Channel::ChannelRequest* request) override;

	private:
		// The time jitter algorithm for vhost.
		RtmpRtmpJitterAlgorithm jitter_algorithm;
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
		// The gop cache for client fast startup.
		RtmpGopCache* gop_cache_;
		// The metadata cache.
		RtmpMetaCache* meta;
		// The format, codec information.
		RtmpRtmpFormat* format_;

	public:
		srs_error_t ConsumerDump(RtmpConsumer* consumer);

		/* Pure virtual methods inherited from RtmpTransport::Listener. */
		bool IsPublishing() override;
		srs_error_t OnMetaData(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata) override;
		srs_error_t OnAudio(RtmpCommonMessage* shared_audio) override;
		srs_error_t OnVideo(RtmpCommonMessage* shared_video) override;
		srs_error_t OnAggregate(RtmpCommonMessage* shared_video) override;

	private:
		srs_error_t OnCreateServerPublisher(RtmpPublisher* publisher);
		srs_error_t on_audio_imp(RtmpSharedPtrMessage* msg);
		srs_error_t on_video_imp(RtmpSharedPtrMessage* msg);

	private:
		void SetNewTransportIdFromData(json& data, std::string& transportId) const;

	private:
		// Passed by argument.
		std::string id;
		RTC::Shared* shared;

		RtmpRequest* req_;
		RtmpPublisher* publisher_;
		// Allocated by this.
		absl::flat_hash_map<std::string, RtmpTransport*> mapTransports_;
		std::unordered_map<std::string, RtmpConsumer*> consumers_;
	};

} // namespace RTMP

#endif