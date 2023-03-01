#define MS_CLASS "RTMP::RtmpPacket"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpPacket.hpp"
#include "Logger.hpp"
#include <memory>

namespace RTMP
{
	RtmpPacket::RtmpPacket()
	{
	}

	RtmpPacket::~RtmpPacket()
	{
	}

	srs_error_t RtmpPacket::to_msg(RtmpCommonMessage* msg, int stream_id)
	{
		srs_error_t err = srs_success;

		int size      = 0;
		char* payload = NULL;
		if ((err = encode(size, payload)) != srs_success)
		{
			return srs_error_wrap(err, "encode packet");
		}

		// encode packet to payload and size.
		if (size <= 0 || payload == NULL)
		{
			MS_WARN_DEV("packet is empty, ignore empty message.");
			return err;
		}

		// to message
		RtmpMessageHeader header;
		header.payload_length = size;
		header.message_type   = get_message_type();
		header.stream_id      = stream_id;
		header.perfer_cid     = get_prefer_cid();

		if ((err = msg->create(&header, payload, size)) != srs_success)
		{
			return srs_error_wrap(err, "create %dB message", size);
		}

		return err;
	}

	srs_error_t RtmpPacket::encode(int& psize, char*& ppayload)
	{
		srs_error_t err = srs_success;

		int size      = get_size();
		char* payload = NULL;

		if (size > 0)
		{
			payload = new char[size];

			Utils::RtmpBuffer* stream = new Utils::RtmpBuffer(payload, size);
			std::unique_ptr<Utils::RtmpBuffer> streamPtr{ stream };

			if ((err = encode_packet(stream)) != srs_success)
			{
				FREEA(payload);
				return srs_error_wrap(err, "encode packet");
			}
		}

		psize    = size;
		ppayload = payload;

		return err;
	}

	srs_error_t RtmpPacket::decode(Utils::RtmpBuffer* stream)
	{
		return srs_error_new(ERROR_SYSTEM_PACKET_INVALID, "decode");
	}

	int RtmpPacket::get_prefer_cid()
	{
		return 0;
	}

	int RtmpPacket::get_message_type()
	{
		return 0;
	}

	int RtmpPacket::get_size()
	{
		return 0;
	}

	srs_error_t RtmpPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		return srs_error_new(ERROR_SYSTEM_PACKET_INVALID, "encode");
	}

	RtmpConnectAppPacket::RtmpConnectAppPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_CONNECT;
		transaction_id = 1;
		command_object = SrsAmf0Any::object();
		// optional
		args = NULL;
	}

	RtmpConnectAppPacket::~RtmpConnectAppPacket()
	{
		FREEP(command_object);
		FREEP(args);
	}

	srs_error_t RtmpConnectAppPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_CONNECT)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		// some client donot send id=1.0, so we only warn user if not match.
		if (transaction_id != 1.0)
		{
			MS_WARN_DEV("invalid transaction_id=%.2f", transaction_id);
		}

		if ((err = command_object->read(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if (!stream->empty())
		{
			FREEP(args);

			// see: https://github.com/ossrs/srs/issues/186
			// the args maybe any amf0, for instance, a string. we should drop if not object.
			SrsAmf0Any* any = NULL;
			if ((err = SrsAmf0Any::discovery(stream, &any)) != srs_success)
			{
				return srs_error_wrap(err, "args");
			}
			srs_assert(any);

			// read the instance
			if ((err = any->read(stream)) != srs_success)
			{
				FREEP(any);
				return srs_error_wrap(err, "args");
			}

			// drop when not an AMF0 object.
			if (!any->is_object())
			{
				MS_WARN_DEV("drop the args, see: '4.1.1. connect', marker=%#x", (uint8_t)any->marker);
				FREEP(any);
			}
			else
			{
				args = any->to_object();
			}
		}

		return err;
	}

	int RtmpConnectAppPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpConnectAppPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpConnectAppPacket::get_size()
	{
		int size = 0;

		size += SrsAmf0Size::str(command_name);
		size += SrsAmf0Size::number();
		size += SrsAmf0Size::object(command_object);
		if (args)
		{
			size += SrsAmf0Size::object(args);
		}

		return size;
	}

	srs_error_t RtmpConnectAppPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = command_object->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if (args && (err = args->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		return err;
	}

	RtmpConnectAppResPacket::RtmpConnectAppResPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_RESULT;
		transaction_id = 1;
		props          = SrsAmf0Any::object();
		info           = SrsAmf0Any::object();
	}

	RtmpConnectAppResPacket::~RtmpConnectAppResPacket()
	{
		FREEP(props);
		FREEP(info);
	}

	srs_error_t RtmpConnectAppResPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_RESULT)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		// some client donot send id=1.0, so we only warn user if not match.
		if (transaction_id != 1.0)
		{
			MS_ERROR("invalid transaction_id=%.2f", transaction_id);
		}

		// for RED5(1.0.6), the props is NULL, we must ignore it.
		// @see https://github.com/ossrs/srs/issues/418
		if (!stream->empty())
		{
			SrsAmf0Any* p = NULL;
			if ((err = srs_amf0_read_any(stream, &p)) != srs_success)
			{
				return srs_error_wrap(err, "args");
			}

			// ignore when props is not amf0 object.
			if (!p->is_object())
			{
				MS_WARN_DEV("ignore connect response props marker=%#x.", (uint8_t)p->marker);
				FREEP(p);
			}
			else
			{
				FREEP(props);
				props = p->to_object();
			}
		}

		if ((err = info->read(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		return err;
	}

	int RtmpConnectAppResPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpConnectAppResPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpConnectAppResPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::object(props) +
		       SrsAmf0Size::object(info);
	}

	srs_error_t RtmpConnectAppResPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = props->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "props");
		}

		if ((err = info->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "info");
		}

		return err;
	}

	RtmpCallPacket::RtmpCallPacket()
	{
		command_name   = "";
		transaction_id = 0;
		command_object = NULL;
		arguments      = NULL;
	}

	RtmpCallPacket::~RtmpCallPacket()
	{
		FREEP(command_object);
		FREEP(arguments);
	}

	srs_error_t RtmpCallPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty())
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "empty command_name");
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		FREEP(command_object);
		if ((err = SrsAmf0Any::discovery(stream, &command_object)) != srs_success)
		{
			return srs_error_wrap(err, "discovery command_object");
		}
		if ((err = command_object->read(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if (!stream->empty())
		{
			FREEP(arguments);
			if ((err = SrsAmf0Any::discovery(stream, &arguments)) != srs_success)
			{
				return srs_error_wrap(err, "discovery args");
			}
			if ((err = arguments->read(stream)) != srs_success)
			{
				return srs_error_wrap(err, "read args");
			}
		}

		return err;
	}

	int RtmpCallPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpCallPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpCallPacket::get_size()
	{
		int size = 0;

		size += SrsAmf0Size::str(command_name) + SrsAmf0Size::number();

		if (command_object)
		{
			size += command_object->total_size();
		}

		if (arguments)
		{
			size += arguments->total_size();
		}

		return size;
	}

	srs_error_t RtmpCallPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if (command_object && (err = command_object->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if (arguments && (err = arguments->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		return err;
	}

	RtmpCallResPacket::RtmpCallResPacket(double _transaction_id)
	{
		command_name   = RTMP_AMF0_COMMAND_RESULT;
		transaction_id = _transaction_id;
		command_object = NULL;
		response       = NULL;
	}

	RtmpCallResPacket::~RtmpCallResPacket()
	{
		FREEP(command_object);
		FREEP(response);
	}

	int RtmpCallResPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpCallResPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpCallResPacket::get_size()
	{
		int size = 0;

		size += SrsAmf0Size::str(command_name) + SrsAmf0Size::number();

		if (command_object)
		{
			size += command_object->total_size();
		}

		if (response)
		{
			size += response->total_size();
		}

		return size;
	}

	srs_error_t RtmpCallResPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if (command_object && (err = command_object->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if (response && (err = response->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "response");
		}

		return err;
	}

	RtmpCreateStreamPacket::RtmpCreateStreamPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_CREATE_STREAM;
		transaction_id = 2;
		command_object = SrsAmf0Any::null();
	}

	RtmpCreateStreamPacket::~RtmpCreateStreamPacket()
	{
		FREEP(command_object);
	}

	void RtmpCreateStreamPacket::set_command_object(SrsAmf0Any* v)
	{
		FREEP(command_object);
		command_object = v;
	}

	srs_error_t RtmpCreateStreamPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_CREATE_STREAM)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		return err;
	}

	int RtmpCreateStreamPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpCreateStreamPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpCreateStreamPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null();
	}

	srs_error_t RtmpCreateStreamPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		return err;
	}

	RtmpCreateStreamResPacket::RtmpCreateStreamResPacket(double _transaction_id, double _stream_id)
	{
		command_name   = RTMP_AMF0_COMMAND_RESULT;
		transaction_id = _transaction_id;
		command_object = SrsAmf0Any::null();
		stream_id      = _stream_id;
	}

	RtmpCreateStreamResPacket::~RtmpCreateStreamResPacket()
	{
		FREEP(command_object);
	}

	srs_error_t RtmpCreateStreamResPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_RESULT)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_number(stream, stream_id)) != srs_success)
		{
			return srs_error_wrap(err, "stream_id");
		}

		return err;
	}

	int RtmpCreateStreamResPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpCreateStreamResPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpCreateStreamResPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::number();
	}

	srs_error_t RtmpCreateStreamResPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_write_number(stream, stream_id)) != srs_success)
		{
			return srs_error_wrap(err, "stream_id");
		}

		return err;
	}

	RtmpCloseStreamPacket::RtmpCloseStreamPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_CLOSE_STREAM;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();
	}

	RtmpCloseStreamPacket::~RtmpCloseStreamPacket()
	{
		FREEP(command_object);
	}

	srs_error_t RtmpCloseStreamPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		return err;
	}

	RtmpFMLEStartPacket::RtmpFMLEStartPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_RELEASE_STREAM;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();
	}

	RtmpFMLEStartPacket::~RtmpFMLEStartPacket()
	{
		FREEP(command_object);
	}

	void RtmpFMLEStartPacket::set_command_object(SrsAmf0Any* v)
	{
		FREEP(command_object);
		command_object = v;
	}

	srs_error_t RtmpFMLEStartPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		bool invalid_command_name =
		  (command_name != RTMP_AMF0_COMMAND_RELEASE_STREAM &&
		   command_name != RTMP_AMF0_COMMAND_FC_PUBLISH && command_name != RTMP_AMF0_COMMAND_UNPUBLISH);
		if (command_name.empty() || invalid_command_name)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		return err;
	}

	int RtmpFMLEStartPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpFMLEStartPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpFMLEStartPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::str(stream_name);
	}

	srs_error_t RtmpFMLEStartPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_write_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		return err;
	}

	RtmpFMLEStartPacket* RtmpFMLEStartPacket::create_release_stream(std::string stream)
	{
		RtmpFMLEStartPacket* pkt = new RtmpFMLEStartPacket();

		pkt->command_name   = RTMP_AMF0_COMMAND_RELEASE_STREAM;
		pkt->transaction_id = 2;
		pkt->stream_name    = stream;

		return pkt;
	}

	RtmpFMLEStartPacket* RtmpFMLEStartPacket::create_FC_publish(std::string stream)
	{
		RtmpFMLEStartPacket* pkt = new RtmpFMLEStartPacket();

		pkt->command_name   = RTMP_AMF0_COMMAND_FC_PUBLISH;
		pkt->transaction_id = 3;
		pkt->stream_name    = stream;

		return pkt;
	}

	RtmpFMLEStartResPacket::RtmpFMLEStartResPacket(double _transaction_id)
	{
		command_name   = RTMP_AMF0_COMMAND_RESULT;
		transaction_id = _transaction_id;
		command_object = SrsAmf0Any::null();
		args           = SrsAmf0Any::undefined();
	}

	RtmpFMLEStartResPacket::~RtmpFMLEStartResPacket()
	{
		FREEP(command_object);
		FREEP(args);
	}

	void RtmpFMLEStartResPacket::set_args(SrsAmf0Any* v)
	{
		FREEP(args);
		args = v;
	}

	void RtmpFMLEStartResPacket::set_command_object(SrsAmf0Any* v)
	{
		FREEP(command_object);
		command_object = v;
	}

	srs_error_t RtmpFMLEStartResPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_RESULT)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_undefined(stream)) != srs_success)
		{
			return srs_error_wrap(err, "stream_id");
		}

		return err;
	}

	int RtmpFMLEStartResPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpFMLEStartResPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpFMLEStartResPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::undefined();
	}

	srs_error_t RtmpFMLEStartResPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_write_undefined(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		return err;
	}

	RtmpPublishPacket::RtmpPublishPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_PUBLISH;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();
		type           = "live";
	}

	RtmpPublishPacket::~RtmpPublishPacket()
	{
		FREEP(command_object);
	}

	void RtmpPublishPacket::set_command_object(SrsAmf0Any* v)
	{
		FREEP(command_object);
		command_object = v;
	}

	srs_error_t RtmpPublishPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_PUBLISH)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		if (!stream->empty() && (err = srs_amf0_read_string(stream, type)) != srs_success)
		{
			return srs_error_wrap(err, "publish type");
		}

		return err;
	}

	int RtmpPublishPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpPublishPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpPublishPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::str(stream_name) + SrsAmf0Size::str(type);
	}

	srs_error_t RtmpPublishPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_write_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		if ((err = srs_amf0_write_string(stream, type)) != srs_success)
		{
			return srs_error_wrap(err, "type");
		}

		return err;
	}

	RtmpPausePacket::RtmpPausePacket()
	{
		command_name   = RTMP_AMF0_COMMAND_PAUSE;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();

		time_ms  = 0;
		is_pause = true;
	}

	RtmpPausePacket::~RtmpPausePacket()
	{
		FREEP(command_object);
	}

	srs_error_t RtmpPausePacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_PAUSE)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_boolean(stream, is_pause)) != srs_success)
		{
			return srs_error_wrap(err, "is_pause");
		}

		if ((err = srs_amf0_read_number(stream, time_ms)) != srs_success)
		{
			return srs_error_wrap(err, "time");
		}

		return err;
	}

	RtmpPlayPacket::RtmpPlayPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_PLAY;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();

		start    = -2;
		duration = -1;
		reset    = true;
	}

	RtmpPlayPacket::~RtmpPlayPacket()
	{
		FREEP(command_object);
	}

	srs_error_t RtmpPlayPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}
		if (command_name.empty() || command_name != RTMP_AMF0_COMMAND_PLAY)
		{
			return srs_error_new(ERROR_RTMP_AMF0_DECODE, "invalid command_name=%s", command_name.c_str());
		}

		if ((err = srs_amf0_read_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_read_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_read_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		if (!stream->empty() && (err = srs_amf0_read_number(stream, start)) != srs_success)
		{
			return srs_error_wrap(err, "start");
		}
		if (!stream->empty() && (err = srs_amf0_read_number(stream, duration)) != srs_success)
		{
			return srs_error_wrap(err, "duration");
		}

		if (stream->empty())
		{
			return err;
		}

		SrsAmf0Any* reset_value = NULL;
		if ((err = srs_amf0_read_any(stream, &reset_value)) != srs_success)
		{
			return srs_error_wrap(err, "reset");
		}
		std::unique_ptr<SrsAmf0Any> reset_valuePtr{ reset_value };

		if (reset_value)
		{
			// check if the value is bool or number
			// An optional Boolean value or number that specifies whether
			// to flush any previous playlist
			if (reset_value->is_boolean())
			{
				reset = reset_value->to_boolean();
			}
			else if (reset_value->is_number())
			{
				reset = (reset_value->to_number() != 0);
			}
			else
			{
				return srs_error_new(
				  ERROR_RTMP_AMF0_DECODE, "invalid marker=%#x", (uint8_t)reset_value->marker);
			}
		}

		return err;
	}

	int RtmpPlayPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpPlayPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpPlayPacket::get_size()
	{
		int size = SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		           SrsAmf0Size::str(stream_name);

		if (start != -2 || duration != -1 || !reset)
		{
			size += SrsAmf0Size::number();
		}

		if (duration != -1 || !reset)
		{
			size += SrsAmf0Size::number();
		}

		if (!reset)
		{
			size += SrsAmf0Size::boolean();
		}

		return size;
	}

	srs_error_t RtmpPlayPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = srs_amf0_write_string(stream, stream_name)) != srs_success)
		{
			return srs_error_wrap(err, "stream_name");
		}

		if ((start != -2 || duration != -1 || !reset) && (err = srs_amf0_write_number(stream, start)) != srs_success)
		{
			return srs_error_wrap(err, "start");
		}

		if ((duration != -1 || !reset) && (err = srs_amf0_write_number(stream, duration)) != srs_success)
		{
			return srs_error_wrap(err, "duration");
		}

		if (!reset && (err = srs_amf0_write_boolean(stream, reset)) != srs_success)
		{
			return srs_error_wrap(err, "reset");
		}

		return err;
	}

	RtmpPlayResPacket::RtmpPlayResPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_RESULT;
		transaction_id = 0;
		command_object = SrsAmf0Any::null();
		desc           = SrsAmf0Any::object();
	}

	RtmpPlayResPacket::~RtmpPlayResPacket()
	{
		FREEP(command_object);
		FREEP(desc);
	}

	void RtmpPlayResPacket::set_command_object(SrsAmf0Any* v)
	{
		FREEP(command_object);
		command_object = v;
	}

	void RtmpPlayResPacket::set_desc(SrsAmf0Object* v)
	{
		FREEP(desc);
		desc = v;
	}

	int RtmpPlayResPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpPlayResPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpPlayResPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::object(desc);
	}

	srs_error_t RtmpPlayResPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "command_object");
		}

		if ((err = desc->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "desc");
		}

		return err;
	}

	RtmpOnBWDonePacket::RtmpOnBWDonePacket()
	{
		command_name   = RTMP_AMF0_COMMAND_ON_BW_DONE;
		transaction_id = 0;
		args           = SrsAmf0Any::null();
	}

	RtmpOnBWDonePacket::~RtmpOnBWDonePacket()
	{
		FREEP(args);
	}

	void RtmpOnBWDonePacket::set_args(SrsAmf0Any* v)
	{
		FREEP(args);
		args = v;
	}

	int RtmpOnBWDonePacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection;
	}

	int RtmpOnBWDonePacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpOnBWDonePacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null();
	}

	srs_error_t RtmpOnBWDonePacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		return err;
	}

	RtmpOnStatusCallPacket::RtmpOnStatusCallPacket()
	{
		command_name   = RTMP_AMF0_COMMAND_ON_STATUS;
		transaction_id = 0;
		args           = SrsAmf0Any::null();
		data           = SrsAmf0Any::object();
	}

	RtmpOnStatusCallPacket::~RtmpOnStatusCallPacket()
	{
		FREEP(args);
		FREEP(data);
	}

	void RtmpOnStatusCallPacket::set_args(SrsAmf0Any* v)
	{
		FREEP(args);
		args = v;
	}

	void RtmpOnStatusCallPacket::set_data(SrsAmf0Object* v)
	{
		FREEP(data);
		data = v;
	}

	int RtmpOnStatusCallPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpOnStatusCallPacket::get_message_type()
	{
		return RTMP_MSG_AMF0CommandMessage;
	}

	int RtmpOnStatusCallPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::number() + SrsAmf0Size::null() +
		       SrsAmf0Size::object(data);
	}

	srs_error_t RtmpOnStatusCallPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_number(stream, transaction_id)) != srs_success)
		{
			return srs_error_wrap(err, "transaction_id");
		}

		if ((err = srs_amf0_write_null(stream)) != srs_success)
		{
			return srs_error_wrap(err, "args");
		}

		if ((err = data->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "data");
		}

		return err;
	}

	RtmpOnStatusDataPacket::RtmpOnStatusDataPacket()
	{
		command_name = RTMP_AMF0_COMMAND_ON_STATUS;
		data         = SrsAmf0Any::object();
	}

	RtmpOnStatusDataPacket::~RtmpOnStatusDataPacket()
	{
		FREEP(data);
	}

	void RtmpOnStatusDataPacket::set_data(SrsAmf0Object* v)
	{
		FREEP(data);
		data = v;
	}

	int RtmpOnStatusDataPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpOnStatusDataPacket::get_message_type()
	{
		return RTMP_MSG_AMF0DataMessage;
	}

	int RtmpOnStatusDataPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::object(data);
	}

	srs_error_t RtmpOnStatusDataPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = data->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "data");
		}

		return err;
	}

	RtmpSampleAccessPacket::RtmpSampleAccessPacket()
	{
		command_name        = RTMP_AMF0_DATA_SAMPLE_ACCESS;
		video_sample_access = false;
		audio_sample_access = false;
	}

	RtmpSampleAccessPacket::~RtmpSampleAccessPacket()
	{
	}

	int RtmpSampleAccessPacket::get_prefer_cid()
	{
		return RTMP_CID_OverStream;
	}

	int RtmpSampleAccessPacket::get_message_type()
	{
		return RTMP_MSG_AMF0DataMessage;
	}

	int RtmpSampleAccessPacket::get_size()
	{
		return SrsAmf0Size::str(command_name) + SrsAmf0Size::boolean() + SrsAmf0Size::boolean();
	}

	srs_error_t RtmpSampleAccessPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, command_name)) != srs_success)
		{
			return srs_error_wrap(err, "command_name");
		}

		if ((err = srs_amf0_write_boolean(stream, video_sample_access)) != srs_success)
		{
			return srs_error_wrap(err, "video sample access");
		}

		if ((err = srs_amf0_write_boolean(stream, audio_sample_access)) != srs_success)
		{
			return srs_error_wrap(err, "audio sample access");
		}

		return err;
	}

	RtmpOnMetaDataPacket::RtmpOnMetaDataPacket()
	{
		name     = SRS_CONSTS_RTMP_ON_METADATA;
		metadata = SrsAmf0Any::object();
	}

	RtmpOnMetaDataPacket::~RtmpOnMetaDataPacket()
	{
		FREEP(metadata);
	}

	void RtmpOnMetaDataPacket::set_metadata(SrsAmf0Object* v)
	{
		FREEP(metadata);
		metadata = v;
	}

	srs_error_t RtmpOnMetaDataPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_read_string(stream, name)) != srs_success)
		{
			return srs_error_wrap(err, "name");
		}

		// ignore the @setDataFrame
		if (name == SRS_CONSTS_RTMP_SET_DATAFRAME)
		{
			if ((err = srs_amf0_read_string(stream, name)) != srs_success)
			{
				return srs_error_wrap(err, "name");
			}
		}

		// Allows empty body metadata.
		if (stream->empty())
		{
			return err;
		}

		// the metadata maybe object or ecma array
		SrsAmf0Any* any = NULL;
		if ((err = srs_amf0_read_any(stream, &any)) != srs_success)
		{
			return srs_error_wrap(err, "metadata");
		}

		srs_assert(any);
		if (any->is_object())
		{
			FREEP(metadata);
			metadata = any->to_object();
			return err;
		}

		std::unique_ptr<SrsAmf0Any> anyPtr{ any };

		if (any->is_ecma_array())
		{
			SrsAmf0EcmaArray* arr = any->to_ecma_array();

			// if ecma array, copy to object.
			for (int i = 0; i < arr->count(); i++)
			{
				metadata->set(arr->key_at(i), arr->value_at(i)->copy());
			}
		}

		return err;
	}

	int RtmpOnMetaDataPacket::get_prefer_cid()
	{
		return RTMP_CID_OverConnection2;
	}

	int RtmpOnMetaDataPacket::get_message_type()
	{
		return RTMP_MSG_AMF0DataMessage;
	}

	int RtmpOnMetaDataPacket::get_size()
	{
		return SrsAmf0Size::str(name) + SrsAmf0Size::object(metadata);
	}

	srs_error_t RtmpOnMetaDataPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if ((err = srs_amf0_write_string(stream, name)) != srs_success)
		{
			return srs_error_wrap(err, "name");
		}

		if ((err = metadata->write(stream)) != srs_success)
		{
			return srs_error_wrap(err, "metadata");
		}

		return err;
	}

	RtmpSetWindowAckSizePacket::RtmpSetWindowAckSizePacket()
	{
		ackowledgement_window_size = 0;
	}

	RtmpSetWindowAckSizePacket::~RtmpSetWindowAckSizePacket()
	{
	}

	srs_error_t RtmpSetWindowAckSizePacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 4 only %d bytes", stream->left());
		}

		ackowledgement_window_size = stream->read_4bytes();

		return err;
	}

	int RtmpSetWindowAckSizePacket::get_prefer_cid()
	{
		return RTMP_CID_ProtocolControl;
	}

	int RtmpSetWindowAckSizePacket::get_message_type()
	{
		return RTMP_MSG_WindowAcknowledgementSize;
	}

	int RtmpSetWindowAckSizePacket::get_size()
	{
		return 4;
	}

	srs_error_t RtmpSetWindowAckSizePacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_ENCODE, "requires 4 only %d bytes", stream->left());
		}

		stream->write_4bytes(ackowledgement_window_size);

		return err;
	}

	RtmpAcknowledgementPacket::RtmpAcknowledgementPacket()
	{
		sequence_number = 0;
	}

	RtmpAcknowledgementPacket::~RtmpAcknowledgementPacket()
	{
	}

	srs_error_t RtmpAcknowledgementPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 4 only %d bytes", stream->left());
		}

		sequence_number = (uint32_t)stream->read_4bytes();

		return err;
	}

	int RtmpAcknowledgementPacket::get_prefer_cid()
	{
		return RTMP_CID_ProtocolControl;
	}

	int RtmpAcknowledgementPacket::get_message_type()
	{
		return RTMP_MSG_Acknowledgement;
	}

	int RtmpAcknowledgementPacket::get_size()
	{
		return 4;
	}

	srs_error_t RtmpAcknowledgementPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_ENCODE, "requires 4 only %d bytes", stream->left());
		}

		stream->write_4bytes(sequence_number);

		return err;
	}

	RtmpSetChunkSizePacket::RtmpSetChunkSizePacket()
	{
		chunk_size = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
	}

	RtmpSetChunkSizePacket::~RtmpSetChunkSizePacket()
	{
	}

	srs_error_t RtmpSetChunkSizePacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 4 only %d bytes", stream->left());
		}

		chunk_size = stream->read_4bytes();

		return err;
	}

	int RtmpSetChunkSizePacket::get_prefer_cid()
	{
		return RTMP_CID_ProtocolControl;
	}

	int RtmpSetChunkSizePacket::get_message_type()
	{
		return RTMP_MSG_SetChunkSize;
	}

	int RtmpSetChunkSizePacket::get_size()
	{
		return 4;
	}

	srs_error_t RtmpSetChunkSizePacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(4))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_ENCODE, "requires 4 only %d bytes", stream->left());
		}

		stream->write_4bytes(chunk_size);

		return err;
	}

	RtmpSetPeerBandwidthPacket::RtmpSetPeerBandwidthPacket()
	{
		bandwidth = 0;
		type      = RtmpPeerBandwidthDynamic;
	}

	RtmpSetPeerBandwidthPacket::~RtmpSetPeerBandwidthPacket()
	{
	}

	int RtmpSetPeerBandwidthPacket::get_prefer_cid()
	{
		return RTMP_CID_ProtocolControl;
	}

	int RtmpSetPeerBandwidthPacket::get_message_type()
	{
		return RTMP_MSG_SetPeerBandwidth;
	}

	int RtmpSetPeerBandwidthPacket::get_size()
	{
		return 5;
	}

	srs_error_t RtmpSetPeerBandwidthPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(5))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_ENCODE, "requires 5 only %d bytes", stream->left());
		}

		stream->write_4bytes(bandwidth);
		stream->write_1bytes(type);

		return err;
	}

	RtmpUserControlPacket::RtmpUserControlPacket()
	{
		event_type = 0;
		event_data = 0;
		extra_data = 0;
	}

	RtmpUserControlPacket::~RtmpUserControlPacket()
	{
	}

	srs_error_t RtmpUserControlPacket::decode(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(2))
		{
			return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 2 only %d bytes", stream->left());
		}

		event_type = stream->read_2bytes();

		if (event_type == RtmpPCUCFmsEvent0)
		{
			if (!stream->require(1))
			{
				return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 1 only %d bytes", stream->left());
			}
			event_data = stream->read_1bytes();
		}
		else
		{
			if (!stream->require(4))
			{
				return srs_error_new(ERROR_RTMP_MESSAGE_DECODE, "requires 4 only %d bytes", stream->left());
			}
			event_data = stream->read_4bytes();
		}

		if (event_type == SrcPCUCSetBufferLength)
		{
			if (!stream->require(4))
			{
				return srs_error_new(ERROR_RTMP_MESSAGE_ENCODE, "requires 4 only %d bytes", stream->left());
			}
			extra_data = stream->read_4bytes();
		}

		return err;
	}

	int RtmpUserControlPacket::get_prefer_cid()
	{
		return RTMP_CID_ProtocolControl;
	}

	int RtmpUserControlPacket::get_message_type()
	{
		return RTMP_MSG_UserControlMessage;
	}

	int RtmpUserControlPacket::get_size()
	{
		int size = 2;

		if (event_type == RtmpPCUCFmsEvent0)
		{
			size += 1;
		}
		else
		{
			size += 4;
		}

		if (event_type == SrcPCUCSetBufferLength)
		{
			size += 4;
		}

		return size;
	}

	srs_error_t RtmpUserControlPacket::encode_packet(Utils::RtmpBuffer* stream)
	{
		srs_error_t err = srs_success;

		if (!stream->require(get_size()))
		{
			return srs_error_new(
			  ERROR_RTMP_MESSAGE_ENCODE, "requires %d only %d bytes", get_size(), stream->left());
		}

		stream->write_2bytes(event_type);

		if (event_type == RtmpPCUCFmsEvent0)
		{
			stream->write_1bytes(event_data);
		}
		else
		{
			stream->write_4bytes(event_data);
		}

		// when event type is set buffer length,
		// write the extra buffer length.
		if (event_type == SrcPCUCSetBufferLength)
		{
			stream->write_4bytes(extra_data);
		}

		return err;
	}

} // namespace RTMP
