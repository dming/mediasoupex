#define MS_CLASS "RTMP::RtmpRouter"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpRouter.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpConsumer.hpp"
#include "RTMP/RtmpPublisher.hpp"
#include "RTMP/RtmpServerSession.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTMP/RtmpUtility.hpp"
#include "RTC/TransportTuple.hpp"
#include <algorithm>
#include <sstream>

// for 26ms per audio packet,
// 115 packets is 3s.
#define SRS_PURE_AUDIO_GUESS_COUNT 115

// when got these videos or audios, pure audio or video, mix ok.
#define SRS_MIX_CORRECT_PURE_AV 10

namespace RTMP
{

	RtmpMixQueue::RtmpMixQueue()
	{
		nb_videos = 0;
		nb_audios = 0;
	}

	RtmpMixQueue::~RtmpMixQueue()
	{
		clear();
	}

	void RtmpMixQueue::clear()
	{
		std::multimap<int64_t, RtmpSharedPtrMessage*>::iterator it;
		for (it = msgs.begin(); it != msgs.end(); ++it)
		{
			RtmpSharedPtrMessage* msg = it->second;
			FREEP(msg);
		}
		msgs.clear();

		nb_videos = 0;
		nb_audios = 0;
	}

	void RtmpMixQueue::push(RtmpSharedPtrMessage* msg)
	{
		msgs.insert(std::make_pair(msg->timestamp, msg));

		if (msg->is_video())
		{
			nb_videos++;
		}
		else
		{
			nb_audios++;
		}
	}

	RtmpSharedPtrMessage* RtmpMixQueue::pop()
	{
		bool mix_ok = false;

		// pure video
		if (nb_videos >= SRS_MIX_CORRECT_PURE_AV && nb_audios == 0)
		{
			mix_ok = true;
		}

		// pure audio
		if (nb_audios >= SRS_MIX_CORRECT_PURE_AV && nb_videos == 0)
		{
			mix_ok = true;
		}

		// got 1 video and 1 audio, mix ok.
		if (nb_videos >= 1 && nb_audios >= 1)
		{
			mix_ok = true;
		}

		if (!mix_ok)
		{
			return nullptr;
		}

		// pop the first msg.
		std::multimap<int64_t, RtmpSharedPtrMessage*>::iterator it = msgs.begin();
		RtmpSharedPtrMessage* msg                                  = it->second;
		msgs.erase(it);

		if (msg->is_video())
		{
			nb_videos--;
		}
		else
		{
			nb_audios--;
		}

		return msg;
	}

	RtmpMetaCache::RtmpMetaCache()
	{
		meta = video = audio = nullptr;
		previous_video = previous_audio = nullptr;
		vformat                         = new RtmpRtmpFormat();
		aformat                         = new RtmpRtmpFormat();
	}

	RtmpMetaCache::~RtmpMetaCache()
	{
		dispose();
		FREEP(vformat);
		FREEP(aformat);
	}

	void RtmpMetaCache::dispose()
	{
		clear();
		FREEP(previous_video);
		FREEP(previous_audio);
	}

	void RtmpMetaCache::clear()
	{
		FREEP(meta);
		FREEP(video);
		FREEP(audio);
	}

	RtmpSharedPtrMessage* RtmpMetaCache::data()
	{
		return meta;
	}

	RtmpSharedPtrMessage* RtmpMetaCache::vsh()
	{
		return video;
	}

	SrsFormat* RtmpMetaCache::vsh_format()
	{
		return vformat;
	}

	RtmpSharedPtrMessage* RtmpMetaCache::ash()
	{
		return audio;
	}

	SrsFormat* RtmpMetaCache::ash_format()
	{
		return aformat;
	}

	srs_error_t RtmpMetaCache::dumps(
	  RtmpConsumer* consumer, bool atc, RtmpRtmpJitterAlgorithm ag, bool dm, bool ds)
	{
		srs_error_t err = srs_success;

		// copy metadata.
		if (dm && meta && (err = consumer->enqueue(meta, atc, ag)) != srs_success)
		{
			return srs_error_wrap(err, "enqueue metadata");
		}

		// copy sequence header
		// copy audio sequence first, for hls to fast parse the "right" audio codec.
		// @see https://github.com/ossrs/srs/issues/301
		if (aformat && aformat->acodec && aformat->acodec->id != SrsAudioCodecIdMP3)
		{
			if (ds && audio)
			{
				MS_DEBUG_DEV_STD("send audio aformat->acodec->id=%d", aformat->acodec->id);
				if ((err = consumer->enqueue(audio, atc, ag)) != srs_success)
				{
					return srs_error_wrap(err, "enqueue audio sh");
				}
			}
		}

		if (ds && video)
		{
			MS_DEBUG_DEV_STD("send video");
			if ((err = consumer->enqueue(video, atc, ag)) != srs_success)
			{
				return srs_error_wrap(err, "enqueue video sh");
			}
		}

		return err;
	}

	RtmpSharedPtrMessage* RtmpMetaCache::previous_vsh()
	{
		return previous_video;
	}

	RtmpSharedPtrMessage* RtmpMetaCache::previous_ash()
	{
		return previous_audio;
	}

	void RtmpMetaCache::update_previous_vsh()
	{
		FREEP(previous_video);
		previous_video = video ? video->copy() : nullptr;
	}

	void RtmpMetaCache::update_previous_ash()
	{
		FREEP(previous_audio);
		previous_audio = audio ? audio->copy() : nullptr;
	}

	srs_error_t RtmpMetaCache::update_data(
	  RtmpMessageHeader* header, RtmpOnMetaDataPacket* metadata, bool& updated)
	{
		updated = false;

		srs_error_t err = srs_success;

		RtmpAmf0Any* prop = nullptr;

		// when exists the duration, remove it to make ExoPlayer happy.
		if (metadata->metadata->get_property("duration") != nullptr)
		{
			metadata->metadata->remove("duration");
		}

		// generate metadata info to print
		std::stringstream ss;
		if ((prop = metadata->metadata->ensure_property_number("width")) != nullptr)
		{
			ss << ", width=" << (int)prop->to_number();
		}
		if ((prop = metadata->metadata->ensure_property_number("height")) != nullptr)
		{
			ss << ", height=" << (int)prop->to_number();
		}
		if ((prop = metadata->metadata->ensure_property_number("videocodecid")) != nullptr)
		{
			ss << ", vcodec=" << (int)prop->to_number();
		}
		if ((prop = metadata->metadata->ensure_property_number("audiocodecid")) != nullptr)
		{
			ss << ", acodec=" << (int)prop->to_number();
		}
		MS_DEBUG_DEV_STD("got metadata%s", ss.str().c_str());

		// add server info to metadata
		metadata->metadata->set("server", RtmpAmf0Any::str(RTMP_SIG_SRS_SERVER));

		// version, for example, 1.0.0
		// add version to metadata, please donot remove it, for debug.
		metadata->metadata->set("server_version", RtmpAmf0Any::str(RTMP_SIG_SRS_VERSION));

		// encode the metadata to payload
		int size      = 0;
		char* payload = nullptr;
		if ((err = metadata->encode(size, payload)) != srs_success)
		{
			return srs_error_wrap(err, "encode metadata");
		}

		if (size <= 0)
		{
			MS_WARN_DEV("ignore the invalid metadata. size=%d", size);
			return err;
		}

		// create a shared ptr message.
		FREEP(meta);
		meta    = new RtmpSharedPtrMessage();
		updated = true;

		// dump message to shared ptr message.
		// the payload/size managed by cache_metadata, user should not free it.
		if ((err = meta->create(header, payload, size)) != srs_success)
		{
			return srs_error_wrap(err, "create metadata");
		}

		return err;
	}

	srs_error_t RtmpMetaCache::update_ash(RtmpSharedPtrMessage* msg)
	{
		FREEP(audio);
		audio = msg->copy();
		update_previous_ash();
		return aformat->on_audio(msg);
	}

	srs_error_t RtmpMetaCache::update_vsh(RtmpSharedPtrMessage* msg)
	{
		FREEP(video);
		video = msg->copy();
		update_previous_vsh();
		return vformat->on_video(msg);
	}

	RtmpGopCache::RtmpGopCache()
	{
		cached_video_count           = 0;
		enable_gop_cache             = true;
		audio_after_last_video_count = 0;
		gop_cache_max_frames_        = 0;
	}

	RtmpGopCache::~RtmpGopCache()
	{
		clear();
	}

	void RtmpGopCache::dispose()
	{
		clear();
	}

	void RtmpGopCache::set(bool v)
	{
		enable_gop_cache = v;

		if (!v)
		{
			clear();
			return;
		}
	}

	void RtmpGopCache::set_gop_cache_max_frames(int v)
	{
		gop_cache_max_frames_ = v;
	}

	bool RtmpGopCache::enabled()
	{
		return enable_gop_cache;
	}

	srs_error_t RtmpGopCache::cache(RtmpSharedPtrMessage* shared_msg)
	{
		srs_error_t err = srs_success;

		if (!enable_gop_cache)
		{
			return err;
		}

		// the gop cache know when to gop it.
		RtmpSharedPtrMessage* msg = shared_msg;

		// got video, update the video count if acceptable
		if (msg->is_video())
		{
			// Drop video when not h.264 or h.265.
			bool codec_ok = RtmpFlvVideo::h264(msg->payload, msg->size);
#ifdef SRS_H265
			codec_ok = codec_ok ? true : RtmpFlvVideo::hevc(msg->payload, msg->size);
#endif
			if (!codec_ok)
			{
				MS_ERROR_STD("Drop video when not h.264 or h.265.");
				return err;
			}

			cached_video_count++;
			audio_after_last_video_count = 0;
		}

		// no acceptable video or pure audio, disable the cache.
		if (pure_audio())
		{
			return err;
		}

		// ok, gop cache enabled, and got an audio.
		if (msg->is_audio())
		{
			audio_after_last_video_count++;
		}

		// clear gop cache when pure audio count overflow
		if (audio_after_last_video_count > SRS_PURE_AUDIO_GUESS_COUNT)
		{
			MS_WARN_DEV_STD("clear gop cache for guess pure audio overflow");
			clear();
			return err;
		}

		// clear gop cache when got key frame
		if (msg->is_video() && RtmpFlvVideo::keyframe(msg->payload, msg->size))
		{
			// MS_DEBUG_DEV_STD("Gop cache keyframe and clear before size=%" PRIu64, gop_cache.size());
			clear();

			// curent msg is video frame, so we set to 1.
			cached_video_count = 1;
		}

		// cache the frame.
		gop_cache.push_back(msg->copy());

		// Clear gop cache if exceed the max frames.
		if (gop_cache_max_frames_ > 0 && gop_cache.size() > (size_t)gop_cache_max_frames_)
		{
			MS_WARN_DEV_STD(
			  "Gop cache exceed max frames=%d, total=%d, videos=%d, aalvc=%d",
			  gop_cache_max_frames_,
			  (int)gop_cache.size(),
			  cached_video_count,
			  audio_after_last_video_count);
			clear();
		}

		return err;
	}

	void RtmpGopCache::clear()
	{
		// MS_DEBUG_DEV_STD("RtmpGopCache::clear");
		std::vector<RtmpSharedPtrMessage*>::iterator it;
		for (it = gop_cache.begin(); it != gop_cache.end(); ++it)
		{
			RtmpSharedPtrMessage* msg = *it;
			FREEP(msg);
		}
		gop_cache.clear();

		cached_video_count           = 0;
		audio_after_last_video_count = 0;
	}

	srs_error_t RtmpGopCache::dump(RtmpConsumer* consumer, bool atc, RtmpRtmpJitterAlgorithm jitter_algorithm)
	{
		srs_error_t err = srs_success;

		std::vector<RtmpSharedPtrMessage*>::iterator it;
		for (it = gop_cache.begin(); it != gop_cache.end(); ++it)
		{
			RtmpSharedPtrMessage* msg = *it;
			if ((err = consumer->enqueue(msg, atc, jitter_algorithm)) != srs_success)
			{
				return srs_error_wrap(err, "enqueue message");
			}
		}
		MS_DEBUG_DEV_STD(
		  "dispatch cached gop success. count=%d, duration=%" PRId64,
		  (int)gop_cache.size(),
		  consumer->get_time());

		return err;
	}

	bool RtmpGopCache::empty()
	{
		return gop_cache.empty();
	}

	srs_utime_t RtmpGopCache::start_time()
	{
		if (empty())
		{
			return 0;
		}

		RtmpSharedPtrMessage* msg = gop_cache[0];
		srs_assert(msg);

		return srs_utime_t(msg->timestamp * SRS_UTIME_MILLISECONDS);
	}

	bool RtmpGopCache::pure_audio()
	{
		return cached_video_count == 0;
	}

	RtmpRouter::RtmpRouter() : req_(nullptr), publisher_(nullptr)
	{
		jitter_algorithm = RtmpRtmpJitterAlgorithmOFF;

		mix_correct = false;
		mix_queue   = new RtmpMixQueue();

		gop_cache_ = new RtmpGopCache();

		meta    = new RtmpMetaCache();
		format_ = new RtmpRtmpFormat();

		is_monotonically_increase = false;
		last_packet_time          = 0;

		atc = false;
	}

	RtmpRouter::~RtmpRouter()
	{
		FREEP(req_);
		FREEP(gop_cache_);
		FREEP(meta);
		FREEP(format_);
		FREEP(mix_queue);
	}

	srs_error_t RtmpRouter::initualize(RtmpRequest* req)
	{
		srs_error_t err = srs_success;
		MS_DEBUG_DEV_STD("initualize req stream url=%s", req->get_stream_url().c_str());
		req_ = req->copy();
		MS_DEBUG_DEV_STD("initualize done. req stream url=%s", req->get_stream_url().c_str());

		jitter_algorithm = RtmpRtmpJitterAlgorithmOFF; // TODO: read from conf
		return err;
	}

	srs_error_t RtmpRouter::OnCreateServerPublisher(RtmpPublisher* publisher)
	{
		MS_DEBUG_DEV_STD("OnCreateServerPublisher start");
		srs_error_t err           = srs_success;
		publisher_                = publisher;
		is_monotonically_increase = true;
		last_packet_time          = 0;
		gop_cache_->clear();
		meta->dispose();
		return err;
	}

	bool RtmpRouter::IsPublishing()
	{
		return publisher_ != nullptr;
	}

	srs_error_t RtmpRouter::CreateServerTransport(
	  RtmpServerSession* session, bool isPublisher, RtmpServerTransport** transport)
	{
		MS_DEBUG_DEV_STD("CreateServerTransport start");
		srs_error_t err         = srs_success;
		RtmpServerTransport* st = nullptr;
		if (isPublisher)
		{
			if (publisher_)
			{
				MS_ERROR_STD("publisher_ already in router , %s", session->GetStreamUrl().c_str());
				return srs_error_new(ERROR_RTMP_SOUP_ERROR, "publisher_ already in router");
			}

			st = new RtmpServerTransport(this, isPublisher, session);
			this->OnCreateServerPublisher(st->GetPublisher());
		}
		else
		{
			RtmpTcpConnection* connection = session->GetConnection();
			if (!connection)
			{
				return srs_error_new(ERROR_RTMP_SOUP_ERROR, "connection not exist");
			}
			RTC::TransportTuple tuple(connection);
			if (consumers_.find(tuple.hash) != consumers_.end())
			{
				return srs_error_new(ERROR_RTMP_SOUP_ERROR, "consumer already exist");
			}
			st                     = new RtmpServerTransport(this, isPublisher, session);
			consumers_[tuple.hash] = st->GetConsumer();
		}

		transports_.push_back(st);
		*transport = st;
		MS_DEBUG_DEV_STD("StreamURL: %s, isPublisher:%d", session->GetStreamUrl().c_str(), isPublisher);
		return err;
	}

	srs_error_t RtmpRouter::RemoveServerSession(RtmpServerSession* session)
	{
		srs_error_t err                      = srs_success;
		RtmpServerTransport* serverTransport = nullptr;
		for (auto it = transports_.begin(); it != transports_.end(); it++)
		{
			if ((serverTransport = dynamic_cast<RtmpServerTransport*>(*it)))
			{
				if (session == serverTransport->GetSession())
				{
					if (serverTransport->IsPublisher() && publisher_ == serverTransport->GetPublisher())
					{
						MS_DEBUG_DEV_STD("RemoveServerSession remove Publisher");
						publisher_ = nullptr;
						gop_cache_->clear();
						meta->dispose();
					}
					else if (serverTransport->IsConsumer())
					{
						RtmpTcpConnection* connection = session->GetConnection();
						RTC::TransportTuple tuple(connection);
						if (consumers_.find(tuple.hash) != consumers_.end())
						{
							MS_DEBUG_DEV_STD("RemoveServerSession remove Consumer tuple.hash=%" PRIu64, tuple.hash);
							RtmpConsumer* consumer = consumers_[tuple.hash];
							consumers_.erase(tuple.hash);
						}
					}
					transports_.erase(it);
					FREEP(serverTransport); // will delete publisher consumer
					return err;
				}
			}
		}
		MS_DEBUG_DEV_STD("cannot remove, session not exist in router. ");
		return srs_error_new(ERROR_RTMP_SOUP_ERROR, "session not exist");
	}

	srs_error_t RtmpRouter::ConsumerDump(RtmpConsumer* consumer)
	{
		// todo: 当consumer生效后，将缓存的信息发送给consumer
		srs_error_t err = srs_success;
		if (publisher_)
		{
			// Copy metadata and sequence header to consumer.
			bool dm = true;
			bool ds = true;
			if ((err = meta->dumps(consumer, atc, jitter_algorithm, dm, ds)) != srs_success)
			{
				return srs_error_wrap(err, "meta dumps");
			}

			// copy gop cache to client.
			if ((err = gop_cache_->dump(consumer, atc, jitter_algorithm)) != srs_success)
			{
				return srs_error_wrap(err, "gop cache dumps");
			}
		}
		return err;
	}

	srs_error_t RtmpRouter::OnAudio(RtmpCommonMessage* shared_audio)
	{
		srs_error_t err = srs_success;

		// Detect where stream is monotonically increasing.
		if (!mix_correct && is_monotonically_increase)
		{
			if (last_packet_time > 0 && shared_audio->header.timestamp < last_packet_time)
			{
				is_monotonically_increase = false;
				MS_WARN_DEV_STD(
				  "AUDIO: Timestamp %" PRId64 "=>%" PRId64 ", may need mix_correct.",
				  last_packet_time,
				  shared_audio->header.timestamp);
			}
		}
		last_packet_time = shared_audio->header.timestamp;

		// convert shared_audio to msg, user should not use shared_audio again.
		// the payload is transfer to msg, and set to nullptr in shared_audio.
		RtmpSharedPtrMessage msg;
		if ((err = msg.create(shared_audio)) != srs_success)
		{
			return srs_error_wrap(err, "create message");
		}

		// directly process the audio message.
		if (!mix_correct)
		{
			return on_audio_imp(&msg);
		}

		// insert msg to the queue.
		mix_queue->push(msg.copy());

		// fetch someone from mix queue.
		RtmpSharedPtrMessage* m = mix_queue->pop();
		if (!m)
		{
			return err;
		}

		// consume the monotonically increase message.
		if (m->is_audio())
		{
			err = on_audio_imp(m);
		}
		else
		{
			err = on_video_imp(m);
		}
		FREEP(m);

		return err;
	}

	srs_error_t RtmpRouter::on_audio_imp(RtmpSharedPtrMessage* msg)
	{
		srs_error_t err = srs_success;

		// TODO: FIXME: Support parsing OPUS for RTC.
		if ((err = format_->on_audio(msg)) != srs_success)
		{
			return srs_error_wrap(err, "format consume audio");
		}

		// Ignore if no format->acodec, it means the codec is not parsed, or unsupport/unknown codec
		// such as G.711 codec
		if (!format_->acodec)
		{
			return err;
		}

		// Whether current packet is sequence header. Note that MP3 does not have one, but we use the
		// first packet as it.
		bool is_sequence_header = format_->is_aac_sequence_header() || format_->is_mp3_sequence_header();

		// whether consumer should drop for the duplicated sequence header.
		bool drop_for_reduce = false;
		if (
		  is_sequence_header && meta->previous_ash()
		  // && _srs_config->get_reduce_sequence_header(req->vhost)
		)
		{
			if (meta->previous_ash()->size == msg->size)
			{
				drop_for_reduce = srs_bytes_equals(meta->previous_ash()->payload, msg->payload, msg->size);
				MS_WARN_DEV_STD(
				  "drop for reduce sh audio, size=%d, drop_for_reduce=%d", msg->size, drop_for_reduce);
			}
		}

		// // Copy to hub to all utilities.
		// if ((err = hub->on_audio(msg)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "consume audio");
		// }

		// // For bridge to consume the message.
		// if (bridge_ && (err = bridge_->on_audio(msg)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "bridge consume audio");
		// }

		// copy to all consumer
		if (!drop_for_reduce)
		{
			for (auto kv : consumers_)
			{
				RTMP::RtmpConsumer* consumer = kv.second;
				if ((err = consumer->enqueue(msg, atc, jitter_algorithm)) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
			}
		}

		// Refresh the sequence header in metadata.
		if (is_sequence_header || !meta->ash())
		{
			if ((err = meta->update_ash(msg)) != srs_success)
			{
				return srs_error_wrap(err, "meta consume audio");
			}
		}

		// when sequence header, donot push to gop cache and adjust the timestamp.
		if (is_sequence_header)
		{
			return err;
		}

		// cache the last gop packets
		if ((err = gop_cache_->cache(msg)) != srs_success)
		{
			return srs_error_wrap(err, "gop cache consume audio");
		}

		// if atc, update the sequence header to abs time.
		if (atc)
		{
			if (meta->ash())
			{
				meta->ash()->timestamp = msg->timestamp;
			}
			if (meta->data())
			{
				meta->data()->timestamp = msg->timestamp;
			}
		}

		return err;
	}

	srs_error_t RtmpRouter::OnVideo(RtmpCommonMessage* shared_video)
	{
		srs_error_t err = srs_success;

		// Detect where stream is monotonically increasing.
		if (!mix_correct && is_monotonically_increase)
		{
			if (last_packet_time > 0 && shared_video->header.timestamp < last_packet_time)
			{
				is_monotonically_increase = false;
				MS_WARN_DEV_STD(
				  "VIDEO: Timestamp %" PRId64 "=>%" PRId64 ", may need mix_correct.",
				  last_packet_time,
				  shared_video->header.timestamp);
			}
		}
		last_packet_time = shared_video->header.timestamp;

		// drop any unknown header video.
		// @see https://github.com/ossrs/srs/issues/421
		if (!RtmpFlvVideo::acceptable(shared_video->payload, shared_video->size))
		{
			char b0 = 0x00;
			if (shared_video->size > 0)
			{
				b0 = shared_video->payload[0];
			}

			MS_WARN_DEV_STD("drop unknown header video, size=%d, bytes[0]=%#x", shared_video->size, b0);
			return err;
		}

		// convert shared_video to msg, user should not use shared_video again.
		// the payload is transfer to msg, and set to nullptr in shared_video.
		RtmpSharedPtrMessage msg;
		if ((err = msg.create(shared_video)) != srs_success)
		{
			return srs_error_wrap(err, "create message");
		}

		// directly process the video message.
		if (!mix_correct)
		{
			return on_video_imp(&msg);
		}

		// insert msg to the queue.
		mix_queue->push(msg.copy());

		// fetch someone from mix queue.
		RtmpSharedPtrMessage* m = mix_queue->pop();
		if (!m)
		{
			return err;
		}

		// consume the monotonically increase message.
		if (m->is_audio())
		{
			err = on_audio_imp(m);
		}
		else
		{
			err = on_video_imp(m);
		}
		FREEP(m);

		return err;
	}

	srs_error_t RtmpRouter::on_video_imp(RtmpSharedPtrMessage* msg)
	{
		srs_error_t err = srs_success;

		bool is_sequence_header = RtmpFlvVideo::sh(msg->payload, msg->size);

		// user can disable the sps parse to workaround when parse sps failed.
		// @see https://github.com/ossrs/srs/issues/474
		if (is_sequence_header)
		{
			// format_->avc_parse_sps = _srs_config->get_parse_sps(req->vhost);
		}

		if ((err = format_->on_video(msg)) != srs_success)
		{
			return srs_error_wrap(err, "format consume video");
		}

		// Ignore if no format->vcodec, it means the codec is not parsed, or unsupport/unknown codec
		// such as H.263 codec
		if (!format_->vcodec)
		{
			return err;
		}

		// whether consumer should drop for the duplicated sequence header.
		bool drop_for_reduce = false;
		if (
		  is_sequence_header && meta->previous_vsh()
		  //  && _srs_config->get_reduce_sequence_header(req->vhost)
		)
		{
			if (meta->previous_vsh()->size == msg->size)
			{
				drop_for_reduce = srs_bytes_equals(meta->previous_vsh()->payload, msg->payload, msg->size);
				MS_WARN_DEV("drop for reduce sh video, size=%d", msg->size);
			}
		}

		// cache the sequence header if h264
		// donot cache the sequence header to gop_cache, return here.
		if (is_sequence_header && (err = meta->update_vsh(msg)) != srs_success)
		{
			return srs_error_wrap(err, "meta update video");
		}

		// // Copy to hub to all utilities.
		// if ((err = hub->on_video(msg, is_sequence_header)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "hub consume video");
		// }

		// // For bridge to consume the message.
		// if (bridge_ && (err = bridge_->on_video(msg)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "bridge consume video");
		// }

		// copy to all consumer
		if (!drop_for_reduce)
		{
			for (auto kv : consumers_)
			{
				RTMP::RtmpConsumer* consumer = kv.second;
				if ((err = consumer->enqueue(msg, atc, jitter_algorithm)) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
			}
		}

		// when sequence header, donot push to gop cache and adjust the timestamp.
		if (is_sequence_header)
		{
			return err;
		}

		// cache the last gop packets
		if ((err = gop_cache_->cache(msg)) != srs_success)
		{
			return srs_error_wrap(err, "gop cache consume vdieo");
		}

		// if atc, update the sequence header to abs time.
		if (atc)
		{
			if (meta->vsh())
			{
				meta->vsh()->timestamp = msg->timestamp;
			}
			if (meta->data())
			{
				meta->data()->timestamp = msg->timestamp;
			}
		}

		return err;
	}

	srs_error_t RtmpRouter::OnAggregate(RtmpCommonMessage* msg)
	{
		srs_error_t err = srs_success;

		Utils::RtmpBuffer* stream = new Utils::RtmpBuffer(msg->payload, msg->size);
		RtmpAutoFree(Utils::RtmpBuffer, stream);

		// the aggregate message always use abs time.
		int delta = -1;

		while (!stream->empty())
		{
			if (!stream->require(1))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate");
			}
			int8_t type = stream->read_1bytes();

			if (!stream->require(3))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate");
			}
			int32_t data_size = stream->read_3bytes();

			if (data_size < 0)
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate size");
			}

			if (!stream->require(3))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate time");
			}
			int32_t timestamp = stream->read_3bytes();

			if (!stream->require(1))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate time(high bits)");
			}
			int32_t time_h = stream->read_1bytes();

			timestamp |= time_h << 24;
			timestamp &= 0x7FFFFFFF;

			// adjust abs timestamp in aggregate msg.
			// only -1 means uninitialized delta.
			if (delta == -1)
			{
				delta = (int)msg->header.timestamp - (int)timestamp;
			}
			timestamp += delta;

			if (!stream->require(3))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate stream id");
			}
			int32_t stream_id = stream->read_3bytes();

			if (data_size > 0 && !stream->require(data_size))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate data");
			}

			// to common message.
			RtmpCommonMessage o;

			o.header.message_type    = type;
			o.header.payload_length  = data_size;
			o.header.timestamp_delta = timestamp;
			o.header.timestamp       = timestamp;
			o.header.stream_id       = stream_id;
			o.header.perfer_cid      = msg->header.perfer_cid;

			if (data_size > 0)
			{
				o.size    = data_size;
				o.payload = new char[o.size];
				stream->read_bytes(o.payload, o.size);
			}

			if (!stream->require(4))
			{
				return srs_error_new(ERROR_RTMP_AGGREGATE, "aggregate previous tag size");
			}
			stream->read_4bytes();

			// process parsed message
			if (o.header.is_audio())
			{
				if ((err = OnAudio(&o)) != srs_success)
				{
					return srs_error_wrap(err, "consume audio");
				}
			}
			else if (o.header.is_video())
			{
				if ((err = OnVideo(&o)) != srs_success)
				{
					return srs_error_wrap(err, "consume video");
				}
			}
		}

		return err;
	}

	srs_error_t RtmpRouter::OnMetaData(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata)
	{
		srs_error_t err = srs_success;

		if ((err = format_->on_metadata(metadata)) != srs_success)
		{
			return srs_error_wrap(err, "Format parse metadata");
		}

		// if allow atc_auto and bravo-atc detected, open atc for vhost.
		RtmpAmf0Any* prop = NULL;
		// atc               = _srs_config->get_atc(req->vhost);
		// if (_srs_config->get_atc_auto(req->vhost))
		// {
		// 	if ((prop = metadata->metadata->get_property("bravo_atc")) != NULL)
		// 	{
		// 		if (prop->is_string() && prop->to_str() == "true")
		// 		{
		// 			atc = true;
		// 		}
		// 	}
		// }

		// Update the meta cache.
		bool updated = false;
		if ((err = meta->update_data(&msg->header, metadata, updated)) != srs_success)
		{
			return srs_error_wrap(err, "update metadata");
		}
		if (!updated)
		{
			return err;
		}

		// // when already got metadata, drop when reduce sequence header.
		bool drop_for_reduce = false;
		// if (meta->data() && _srs_config->get_reduce_sequence_header(req->vhost))
		// {
		// 	drop_for_reduce = true;
		// 	srs_warn("drop for reduce sh metadata, size=%d", msg->size);
		// }

		// copy to all consumer
		if (!drop_for_reduce)
		{
			for (auto kv : consumers_)
			{
				RTMP::RtmpConsumer* consumer = kv.second;
				if ((err = consumer->enqueue(meta->data(), atc, jitter_algorithm)) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
			}
		}

		// Copy to hub to all utilities.
		// return hub->OnMetaData(meta->data(), metadata);

		return err;
	}
} // namespace RTMP
