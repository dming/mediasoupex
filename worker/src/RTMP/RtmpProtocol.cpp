#define MS_CLASS "RTMP::RtmpPacket"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpProtocol.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpHttp.hpp"
#include "RTMP/RtmpUtility.hpp"

// FMLE
#define RTMP_AMF0_COMMAND_ON_FC_PUBLISH "onFCPublish"
#define RTMP_AMF0_COMMAND_ON_FC_UNPUBLISH "onFCUnpublish"

// default stream id for response the createStream request.
#define SRS_DEFAULT_SID 1

namespace RTMP
{

	RtmpRequest::RtmpRequest()
	{
		objectEncoding = RTMP_SIG_AMF0_VER;
		duration       = -1;
		port           = SRS_CONSTS_RTMP_DEFAULT_PORT;
		args           = NULL;

		protocol = "rtmp";
	}

	RtmpRequest::~RtmpRequest()
	{
		FREEP(args);
	}

	RtmpRequest* RtmpRequest::copy()
	{
		RtmpRequest* cp = new RtmpRequest();

		cp->ip             = ip;
		cp->vhost          = vhost;
		cp->app            = app;
		cp->objectEncoding = objectEncoding;
		cp->pageUrl        = pageUrl;
		cp->host           = host;
		cp->port           = port;
		cp->param          = param;
		cp->schema         = schema;
		cp->stream         = stream;
		cp->swfUrl         = swfUrl;
		cp->tcUrl          = tcUrl;
		cp->duration       = duration;
		if (args)
		{
			cp->args = args->copy()->to_object();
		}

		cp->protocol = protocol;

		return cp;
	}

	void RtmpRequest::update_auth(RtmpRequest* req)
	{
		pageUrl = req->pageUrl;
		swfUrl  = req->swfUrl;
		tcUrl   = req->tcUrl;
		param   = req->param;

		ip             = req->ip;
		vhost          = req->vhost;
		app            = req->app;
		objectEncoding = req->objectEncoding;
		host           = req->host;
		port           = req->port;
		param          = req->param;
		schema         = req->schema;
		duration       = req->duration;

		if (args)
		{
			FREEP(args);
		}
		if (req->args)
		{
			args = req->args->copy()->to_object();
		}

		protocol = req->protocol;

		MS_DEBUG_DEV("update req of soruce for auth ok");
	}

	std::string RtmpRequest::get_stream_url()
	{
		return srs_generate_stream_url(vhost, app, stream);
	}

	void RtmpRequest::strip()
	{
		// remove the unsupported chars in names.
		host   = srs_string_remove(host, "/ \n\r\t");
		vhost  = srs_string_remove(vhost, "/ \n\r\t");
		app    = srs_string_remove(app, " \n\r\t");
		stream = srs_string_remove(stream, " \n\r\t");

		// remove end slash of app/stream
		app    = srs_string_trim_end(app, "/");
		stream = srs_string_trim_end(stream, "/");

		// remove start slash of app/stream
		app    = srs_string_trim_start(app, "/");
		stream = srs_string_trim_start(stream, "/");
	}

	RtmpRequest* RtmpRequest::as_http()
	{
		schema = "http";
		tcUrl  = srs_generate_tc_url(schema, host, vhost, app, port);
		return this;
	}

	RtmpResponse::RtmpResponse()
	{
		stream_id = SRS_DEFAULT_SID;
	}

	RtmpResponse::~RtmpResponse()
	{
	}

	std::string srs_client_type_string(SrsRtmpConnType type)
	{
		switch (type)
		{
			case SrsRtmpConnPlay:
				return "rtmp-play";
			case SrsHlsPlay:
				return "hls-play";
			case SrsFlvPlay:
				return "flv-play";
			case SrsRtcConnPlay:
				return "rtc-play";
			case SrsRtmpConnFlashPublish:
				return "flash-publish";
			case SrsRtmpConnFMLEPublish:
				return "fmle-publish";
			case SrsRtmpConnHaivisionPublish:
				return "haivision-publish";
			case SrsRtcConnPublish:
				return "rtc-publish";
			case SrsSrtConnPlay:
				return "srt-play";
			case SrsSrtConnPublish:
				return "srt-publish";
			default:
				return "Unknown";
		}
	}

	bool srs_client_type_is_publish(SrsRtmpConnType type)
	{
		return (type & 0xff00) == 0x0200;
	}

	RtmpClientInfo::RtmpClientInfo()
	{
		edge = false;
		req  = new RtmpRequest();
		res  = new RtmpResponse();
		type = SrsRtmpConnUnknown;
	}

	RtmpClientInfo::~RtmpClientInfo()
	{
		FREEP(req);
		FREEP(res);
	}

	RtmpProtocol::AckWindowSize::AckWindowSize()
	{
		window          = 0;
		sequence_number = 0;
		nb_recv_bytes   = 0;
	}

	RtmpProtocol::RtmpProtocol(/* args */)
	{
		in_chunk_size    = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
		out_chunk_size   = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
		show_debug_info  = true;
		in_buffer_length = 0;
	}

	RtmpProtocol::~RtmpProtocol()
	{
	}

	srs_error_t RtmpProtocol::decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket)
	{
		*ppacket = NULL;

		srs_error_t err = srs_success;

		srs_assert(msg != NULL);
		srs_assert(msg->payload != NULL);
		srs_assert(msg->size > 0);

		Utils::RtmpBuffer stream(msg->payload, msg->size);

		// decode the packet.
		RtmpPacket* packet = NULL;
		if ((err = do_decode_message(msg->header, &stream, &packet)) != srs_success)
		{
			FREEP(packet);
			return srs_error_wrap(err, "decode message");
		}

		// set to output ppacket only when success.
		*ppacket = packet;

		return err;
	}

	srs_error_t RtmpProtocol::do_decode_message(
	  RtmpMessageHeader& header,
	  Utils::RtmpBuffer* stream,
	  RtmpPacket** ppacket) // [dming] do_decode_message
	{
		srs_error_t err = srs_success;

		RtmpPacket* packet = NULL;

		// decode specified packet type
		if (header.is_amf0_command() || header.is_amf3_command() || header.is_amf0_data() || header.is_amf3_data())
		{
			// skip 1bytes to decode the amf3 command.
			if (header.is_amf3_command() && stream->require(1))
			{
				stream->skip(1);
			}

			// amf0 command message.
			// need to read the command name.
			std::string command;
			if ((err = srs_amf0_read_string(stream, command)) != srs_success)
			{
				return srs_error_wrap(err, "decode command name");
			}

			// result/error packet
			if (command == RTMP_AMF0_COMMAND_RESULT || command == RTMP_AMF0_COMMAND_ERROR)
			{
				double transactionId = 0.0;
				if ((err = srs_amf0_read_number(stream, transactionId)) != srs_success)
				{
					return srs_error_wrap(err, "decode tid for %s", command.c_str());
				}

				// reset stream, for header read completed.
				stream->skip(-1 * stream->pos());
				if (header.is_amf3_command())
				{
					stream->skip(1);
				}

				// find the call name
				if (requests.find(transactionId) == requests.end())
				{
					return srs_error_new(
					  ERROR_RTMP_NO_REQUEST,
					  "find request for command=%s, tid=%.2f",
					  command.c_str(),
					  transactionId);
				}

				std::string request_name = requests[transactionId];
				if (request_name == RTMP_AMF0_COMMAND_CONNECT)
				{
					*ppacket = packet = new RtmpConnectAppResPacket();
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_CREATE_STREAM)
				{
					*ppacket = packet = new RtmpCreateStreamResPacket(0, 0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_RELEASE_STREAM)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_FC_PUBLISH)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else if (request_name == RTMP_AMF0_COMMAND_UNPUBLISH)
				{
					*ppacket = packet = new RtmpFMLEStartResPacket(0);
					return packet->decode(stream);
				}
				else
				{
					return srs_error_new(
					  ERROR_RTMP_NO_REQUEST, "request=%s, tid=%.2f", request_name.c_str(), transactionId);
				}
			}

			// reset to zero(amf3 to 1) to restart decode.
			stream->skip(-1 * stream->pos());
			if (header.is_amf3_command())
			{
				stream->skip(1);
			}

			// decode command object.
			if (command == RTMP_AMF0_COMMAND_CONNECT)
			{
				*ppacket = packet = new RtmpConnectAppPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_CREATE_STREAM)
			{
				*ppacket = packet = new RtmpCreateStreamPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PLAY)
			{
				*ppacket = packet = new RtmpPlayPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PAUSE)
			{
				*ppacket = packet = new RtmpPausePacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_RELEASE_STREAM)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_FC_PUBLISH)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_PUBLISH)
			{
				*ppacket = packet = new RtmpPublishPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_UNPUBLISH)
			{
				*ppacket = packet = new RtmpFMLEStartPacket();
				return packet->decode(stream);
			}
			else if (command == SRS_CONSTS_RTMP_SET_DATAFRAME)
			{
				*ppacket = packet = new RtmpOnMetaDataPacket();
				return packet->decode(stream);
			}
			else if (command == SRS_CONSTS_RTMP_ON_METADATA)
			{
				*ppacket = packet = new RtmpOnMetaDataPacket();
				return packet->decode(stream);
			}
			else if (command == RTMP_AMF0_COMMAND_CLOSE_STREAM)
			{
				*ppacket = packet = new RtmpCloseStreamPacket();
				return packet->decode(stream);
			}
			else if (header.is_amf0_command() || header.is_amf3_command())
			{
				*ppacket = packet = new RtmpCallPacket();
				return packet->decode(stream);
			}

			// default packet to drop message.
			*ppacket = packet = new RtmpPacket();
			return err;
		}
		else if (header.is_user_control_message())
		{
			*ppacket = packet = new RtmpUserControlPacket();
			return packet->decode(stream);
		}
		else if (header.is_window_ackledgement_size())
		{
			*ppacket = packet = new RtmpSetWindowAckSizePacket();
			return packet->decode(stream);
		}
		else if (header.is_ackledgement())
		{
			*ppacket = packet = new RtmpAcknowledgementPacket();
			return packet->decode(stream);
		}
		else if (header.is_set_chunk_size())
		{
			*ppacket = packet = new RtmpSetChunkSizePacket();
			return packet->decode(stream);
		}
		else
		{
			if (!header.is_set_peer_bandwidth() && !header.is_ackledgement())
			{
				MS_DEBUG_DEV("drop unknown message, type=%d", header.message_type);
			}
		}

		return err;
	}

	srs_error_t RtmpProtocol::on_send_packet(RtmpMessageHeader* mh, RtmpPacket* packet)
	{
		srs_error_t err = srs_success;

		// ignore raw bytes oriented RTMP message.
		if (packet == NULL)
		{
			return err;
		}

		switch (mh->message_type)
		{
			case RTMP_MSG_SetChunkSize:
			{
				RtmpSetChunkSizePacket* pkt = dynamic_cast<RtmpSetChunkSizePacket*>(packet);
				out_chunk_size              = pkt->chunk_size;
				break;
			}
			case RTMP_MSG_WindowAcknowledgementSize:
			{
				RtmpSetWindowAckSizePacket* pkt = dynamic_cast<RtmpSetWindowAckSizePacket*>(packet);
				out_ack_size.window             = (uint32_t)pkt->ackowledgement_window_size;
				break;
			}
			case RTMP_MSG_AMF0CommandMessage:
			case RTMP_MSG_AMF3CommandMessage:
			{
				if (true)
				{
					RtmpConnectAppPacket* pkt = dynamic_cast<RtmpConnectAppPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				if (true)
				{
					RtmpCreateStreamPacket* pkt = dynamic_cast<RtmpCreateStreamPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				if (true)
				{
					RtmpFMLEStartPacket* pkt = dynamic_cast<RtmpFMLEStartPacket*>(packet);
					if (pkt)
					{
						requests[pkt->transaction_id] = pkt->command_name;
						break;
					}
				}
				break;
			}
			case RTMP_MSG_VideoMessage:
			case RTMP_MSG_AudioMessage:
				print_debug_info();
			default:
				break;
		}

		return err;
	}

	void RtmpProtocol::print_debug_info()
	{
		if (show_debug_info)
		{
			show_debug_info = false;
			MS_DEBUG_DEV(
			  "protocol in.buffer=%d, in.ack=%d, out.ack=%d, in.chunk=%d, out.chunk=%d",
			  in_buffer_length,
			  in_ack_size.window,
			  out_ack_size.window,
			  in_chunk_size,
			  out_chunk_size);
		}
	}

} // namespace RTMP
