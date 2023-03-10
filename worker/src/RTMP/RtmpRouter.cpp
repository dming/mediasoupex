#define MS_CLASS "RTMP::RtmpRouter"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpRouter.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpSession.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include "RTMP/RtmpUtility.hpp"
#include "RTC/TransportTuple.hpp"
#include <algorithm>
#include <sstream>

#define CONST_MAX_JITTER_MS 250
#define CONST_MAX_JITTER_MS_NEG -250
#define DEFAULT_FRAME_TIME_MS 10

// for 26ms per audio packet,
// 115 packets is 3s.
#define SRS_PURE_AUDIO_GUESS_COUNT 115

// when got these videos or audios, pure audio or video, mix ok.
#define SRS_MIX_CORRECT_PURE_AV 10

namespace RTMP
{

	int srs_time_jitter_string2int(std::string time_jitter)
	{
		if (time_jitter == "full")
		{
			return SrsRtmpJitterAlgorithmFULL;
		}
		else if (time_jitter == "zero")
		{
			return SrsRtmpJitterAlgorithmZERO;
		}
		else
		{
			return SrsRtmpJitterAlgorithmOFF;
		}
	}

	SrsRtmpJitter::SrsRtmpJitter()
	{
		last_pkt_correct_time = -1;
		last_pkt_time         = 0;
	}

	SrsRtmpJitter::~SrsRtmpJitter()
	{
	}

	srs_error_t SrsRtmpJitter::correct(RtmpSharedPtrMessage* msg, SrsRtmpJitterAlgorithm ag)
	{
		srs_error_t err = srs_success;

		// for performance issue
		if (ag != SrsRtmpJitterAlgorithmFULL)
		{
			// all jitter correct features is disabled, ignore.
			if (ag == SrsRtmpJitterAlgorithmOFF)
			{
				return err;
			}

			// start at zero, but donot ensure monotonically increasing.
			if (ag == SrsRtmpJitterAlgorithmZERO)
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

	int64_t SrsRtmpJitter::get_time()
	{
		return last_pkt_correct_time;
	}

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

	SrsMetaCache::SrsMetaCache()
	{
		meta = video = audio = nullptr;
		previous_video = previous_audio = nullptr;
		vformat                         = new SrsRtmpFormat();
		aformat                         = new SrsRtmpFormat();
	}

	SrsMetaCache::~SrsMetaCache()
	{
		dispose();
		FREEP(vformat);
		FREEP(aformat);
	}

	void SrsMetaCache::dispose()
	{
		clear();
		FREEP(previous_video);
		FREEP(previous_audio);
	}

	void SrsMetaCache::clear()
	{
		FREEP(meta);
		FREEP(video);
		FREEP(audio);
	}

	RtmpSharedPtrMessage* SrsMetaCache::data()
	{
		return meta;
	}

	RtmpSharedPtrMessage* SrsMetaCache::vsh()
	{
		return video;
	}

	SrsFormat* SrsMetaCache::vsh_format()
	{
		return vformat;
	}

	RtmpSharedPtrMessage* SrsMetaCache::ash()
	{
		return audio;
	}

	SrsFormat* SrsMetaCache::ash_format()
	{
		return aformat;
	}

	srs_error_t SrsMetaCache::dumps(
	  RtmpConsumer* consumer, bool atc, int streamId, SrsRtmpJitterAlgorithm ag, bool dm, bool ds)
	{
		srs_error_t err = srs_success;

		// copy metadata.
		if (dm && meta && (err = consumer->send_and_free_message(meta->copy(), streamId)) != srs_success)
		{
			return srs_error_wrap(err, "enqueue metadata");
		}

		// copy sequence header
		// copy audio sequence first, for hls to fast parse the "right" audio codec.
		// @see https://github.com/ossrs/srs/issues/301
		if (aformat && aformat->acodec && aformat->acodec->id != SrsAudioCodecIdMP3)
		{
			if (ds && audio && (err = consumer->send_and_free_message(audio->copy(), streamId)) != srs_success)
			{
				return srs_error_wrap(err, "enqueue audio sh");
			}
		}

		if (ds && video && (err = consumer->send_and_free_message(video->copy(), streamId)) != srs_success)
		{
			return srs_error_wrap(err, "enqueue video sh");
		}

		return err;
	}

	RtmpSharedPtrMessage* SrsMetaCache::previous_vsh()
	{
		return previous_video;
	}

	RtmpSharedPtrMessage* SrsMetaCache::previous_ash()
	{
		return previous_audio;
	}

	void SrsMetaCache::update_previous_vsh()
	{
		FREEP(previous_video);
		previous_video = video ? video->copy() : nullptr;
	}

	void SrsMetaCache::update_previous_ash()
	{
		FREEP(previous_audio);
		previous_audio = audio ? audio->copy() : nullptr;
	}

	srs_error_t SrsMetaCache::update_data(
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

	srs_error_t SrsMetaCache::update_ash(RtmpSharedPtrMessage* msg)
	{
		FREEP(audio);
		audio = msg->copy();
		update_previous_ash();
		return aformat->on_audio(msg);
	}

	srs_error_t SrsMetaCache::update_vsh(RtmpSharedPtrMessage* msg)
	{
		FREEP(video);
		video = msg->copy();
		update_previous_vsh();
		return vformat->on_video(msg);
	}

	RtmpRouter::RtmpRouter(/* args */)
	{
		req_             = nullptr;
		jitter_algorithm = SrsRtmpJitterAlgorithmOFF;

		mix_correct      = false;
		mix_queue        = new RtmpMixQueue();
		meta             = new SrsMetaCache();
		format_          = new SrsRtmpFormat();
		last_packet_time = 0;

		atc = false;
	}

	RtmpRouter::~RtmpRouter()
	{
		FREEP(req_);
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
		return err;
	}

	srs_error_t RtmpRouter::CreatePublisher(RtmpSession* session, RtmpPublisher** publisher)
	{
		MS_DEBUG_DEV_STD("CreatePublisher start!!!");
		srs_error_t err = srs_success;
		if (publisher_)
		{
			MS_ERROR_STD(
			  "publisher_ already in router , %s", publisher_->GetSession()->GetStreamUrl().c_str());
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "publisher_ already in router");
		}

		MS_DEBUG_DEV_STD("CreatePublisher start 2 !!!");
		publisher_ = new RtmpPublisher(this, session);
		MS_DEBUG_DEV_STD("CreatePublisher start 3 !!!");
		*publisher = publisher_;
		MS_DEBUG_DEV_STD("CreatePublisher : %s", session->GetStreamUrl().c_str());
		return err;
	}

	srs_error_t RtmpRouter::RemoveSession(RtmpSession* session)
	{
		srs_error_t err = srs_success;
		if (publisher_ && publisher_->GetSession() == session)
		{
			MS_DEBUG_DEV_STD("RemoveSession remove Publisher ");
			FREEP(publisher_);
			return err;
		}

		RtmpTcpConnection* connection = session->GetConnection();
		RTC::TransportTuple tuple(connection);
		if (consumers_.find(tuple.hash) == consumers_.end())
		{
			MS_DEBUG_DEV_STD(
			  "RemoveSession cannot remove Consumer, session not exist.  tuple.hash=%" PRIu64, tuple.hash);
			return srs_error_new(ERROR_RTMP_SOUP_ERROR, "session not exist");
		}
		MS_DEBUG_DEV_STD("RemoveSession remove Consumer tuple.hash=%" PRIu64, tuple.hash);

		RtmpConsumer* consumer = consumers_[tuple.hash];
		consumers_.erase(tuple.hash);
		FREEP(consumer);

		return err;
	}

	srs_error_t RtmpRouter::CreateConsumer(RtmpSession* session, RtmpConsumer*& consumer)
	{
		srs_error_t err               = srs_success;
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
		consumer               = new RtmpConsumer(this, session);
		consumers_[tuple.hash] = consumer;
		return err;
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
			if (
			  (err = meta->dumps(
			     consumer, atc, consumer->GetSession()->GetStreamId, SrsRtmpJitterAlgorithmOFF, dm, ds)) !=
			  srs_success)
			{
				return srs_error_wrap(err, "meta dumps");
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
				MS_WARN_DEV(
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
				MS_WARN_DEV("drop for reduce sh audio, size=%d", msg->size);
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
				if ((err = consumer->send_and_free_message(msg->copy(), consumer->GetSession()->GetStreamId())) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
				// if ((err = consumer->enqueue(msg, atc, jitter_algorithm)) != srs_success)
				// {
				// 	return srs_error_wrap(err, "consume message");
				// }
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

		// // cache the last gop packets
		// if ((err = gop_cache->cache(msg)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "gop cache consume audio");
		// }

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
				MS_WARN_DEV(
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

			MS_WARN_DEV("drop unknown header video, size=%d, bytes[0]=%#x", shared_video->size, b0);
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
				if ((err = consumer->send_and_free_message(msg->copy(), consumer->GetSession()->GetStreamId())) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
				// if ((err = consumer->enqueue(msg, atc, jitter_algorithm)) != srs_success)
				// {
				// 	return srs_error_wrap(err, "consume message");
				// }
			}
		}

		// when sequence header, donot push to gop cache and adjust the timestamp.
		if (is_sequence_header)
		{
			return err;
		}

		// // cache the last gop packets
		// if ((err = gop_cache->cache(msg)) != srs_success)
		// {
		// 	return srs_error_wrap(err, "gop cache consume vdieo");
		// }

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

	srs_error_t RtmpRouter::on_meta_data(RtmpCommonMessage* msg, RtmpOnMetaDataPacket* metadata)
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
				if ((err = consumer->send_and_free_message(meta->data(), consumer->GetSession()->GetStreamId())) != srs_success)
				{
					return srs_error_wrap(err, "consume message");
				}
			}
		}

		// Copy to hub to all utilities.
		// return hub->on_meta_data(meta->data(), metadata);

		return err;
	}
} // namespace RTMP
