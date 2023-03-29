
#define MS_CLASS "RTMP::RtmpParser"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpParser.hpp"
#include "CplxError.hpp"
#include "Logger.hpp"
#include "UtilsBuffer.hpp"

namespace RTMP
{

	srs_error_t RtmpParser::decode_message(RtmpCommonMessage* msg, RtmpPacket** ppacket)
	{
		*ppacket = nullptr;

		srs_error_t err = srs_success;

		srs_assert(msg != nullptr);
		srs_assert(msg->payload != nullptr);
		srs_assert(msg->size > 0);

		Utils::RtmpBuffer stream(msg->payload, msg->size);

		// decode the packet.
		RtmpPacket* packet = nullptr;
		if ((err = do_decode_message(msg->header, &stream, &packet)) != srs_success)
		{
			FREEP(packet);
			return srs_error_wrap(err, "decode message");
		}

		// set to output ppacket only when success.
		*ppacket = packet;

		return err;
	}

	srs_error_t RtmpParser::do_decode_message(
	  RtmpMessageHeader& header,
	  Utils::RtmpBuffer* stream,
	  RtmpPacket** ppacket) // [dming] do_decode_message
	{
		srs_error_t err = srs_success;

		RtmpPacket* packet = nullptr;

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
				// as command is "getStreamLength"
				MS_DEBUG_DEV_STD(
				  "Unknow command, command is %s . just return RtmpCallPacket", command.c_str());
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
				MS_DEBUG_DEV_STD("drop unknown message, type=%d", header.message_type);
			}
		}

		return err;
	}

	srs_error_t RtmpParser::on_send_packet(RtmpMessageHeader* mh, RtmpPacket* packet)
	{
		srs_error_t err = srs_success;

		// ignore raw bytes oriented RTMP message.
		if (packet == nullptr)
		{
			return err;
		}

		switch (mh->message_type)
		{
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
			default:
				break;
		}

		return err;
	}
} // namespace RTMP
