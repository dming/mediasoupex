#ifndef MS_RTMP_PACKET_HPP
#define MS_RTMP_PACKET_HPP

// The amf0 command message, command name macros
#define RTMP_AMF0_COMMAND_CONNECT "connect"
#define RTMP_AMF0_COMMAND_CREATE_STREAM "createStream"
#define RTMP_AMF0_COMMAND_CLOSE_STREAM "closeStream"
#define RTMP_AMF0_COMMAND_PLAY "play"
#define RTMP_AMF0_COMMAND_PAUSE "pause"
#define RTMP_AMF0_COMMAND_ON_BW_DONE "onBWDone"
#define RTMP_AMF0_COMMAND_ON_STATUS "onStatus"
#define RTMP_AMF0_COMMAND_RESULT "_result"
#define RTMP_AMF0_COMMAND_ERROR "_error"
#define RTMP_AMF0_COMMAND_RELEASE_STREAM "releaseStream"
#define RTMP_AMF0_COMMAND_FC_PUBLISH "FCPublish"
#define RTMP_AMF0_COMMAND_UNPUBLISH "FCUnpublish"
#define RTMP_AMF0_COMMAND_PUBLISH "publish"
#define RTMP_AMF0_DATA_SAMPLE_ACCESS "|RtmpSampleAccess"

// The signature for packets to client.
#define RTMP_SIG_FMS_VER "3,5,3,888"
#define RTMP_SIG_AMF0_VER 0
#define RTMP_SIG_CLIENT_ID "ASAICiss"

// The onStatus consts.
#define StatusLevel "level"
#define StatusCode "code"
#define StatusDescription "description"
#define StatusDetails "details"
#define StatusClientId "clientid"
// The status value
#define StatusLevelStatus "status"
// The status error
#define StatusLevelError "error"
// The code value
#define StatusCodeConnectSuccess "NetConnection.Connect.Success"
#define StatusCodeConnectRejected "NetConnection.Connect.Rejected"
#define StatusCodeStreamReset "NetStream.Play.Reset"
#define StatusCodeStreamStart "NetStream.Play.Start"
#define StatusCodeStreamPause "NetStream.Pause.Notify"
#define StatusCodeStreamUnpause "NetStream.Unpause.Notify"
#define StatusCodePublishStart "NetStream.Publish.Start"
#define StatusCodeDataStart "NetStream.Data.Start"
#define StatusCodeUnpublishSuccess "NetStream.Unpublish.Success"

#include "RTMP/RtmpAmf0.hpp"
#include "RTMP/RtmpCommon.hpp"
#include "RTMP/RtmpMessage.hpp"
#include "UtilsBuffer.hpp"
#include <string>

namespace RTMP
{
	// The decoded message payload.
	// @remark we seperate the packet from message,
	//        for the packet focus on logic and domain data,
	//        the message bind to the protocol and focus on protocol, such as header.
	//         we can merge the message and packet, using OOAD hierachy, packet extends from message,
	//         it's better for me to use components -- the message use the packet as payload.
	class RtmpPacket
	{
	public:
		RtmpPacket();
		virtual ~RtmpPacket();

	public:
		// Covert packet to common message.
		virtual srs_error_t to_msg(RtmpCommonMessage* msg, int stream_id);

	public:
		// The subpacket can override this encode,
		// For example, video and audio will directly set the payload withou memory copy,
		// other packet which need to serialize/encode to bytes by override the
		// get_size and encode_packet.
		virtual srs_error_t encode(int& size, char*& payload);
		// Decode functions for concrete packet to override.
	public:
		// The subpacket must override to decode packet from stream.
		// @remark never invoke the super.decode, it always failed.
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		// The cid(chunk id) specifies the chunk to send data over.
		// Generally, each message perfer some cid, for example,
		// all protocol control messages perfer RTMP_CID_ProtocolControl,
		// RtmpSetWindowAckSizePacket is protocol control message.
		virtual int get_prefer_cid();
		// The subpacket must override to provide the right message type.
		// The message type set the RTMP message type in header.
		virtual int get_message_type();

	protected:
		// The subpacket can override to calc the packet size.
		virtual int get_size();
		// The subpacket can override to encode the payload to stream.
		// @remark never invoke the super.encode_packet, it always failed.
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 4.1.1. connect
	// The client sends the connect command to the server to request
	// connection to a server application instance.
	class RtmpConnectAppPacket : public RtmpPacket
	{
	public:
		// Name of the command. Set to "connect".
		std::string command_name;
		// Always set to 1.
		double transaction_id;
		// Command information object which has the name-value pairs.
		// @remark: alloc in packet constructor, user can directly use it,
		//       user should never alloc it again which will cause memory leak.
		// @remark, never be NULL.
		RtmpAmf0Object* command_object;
		// Any optional information
		// @remark, optional, init to and maybe NULL.
		RtmpAmf0Object* args;

	public:
		RtmpConnectAppPacket();
		virtual ~RtmpConnectAppPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};
	// Response  for RtmpConnectAppPacket.
	class RtmpConnectAppResPacket : public RtmpPacket
	{
	public:
		// The _result or _error; indicates whether the response is result or error.
		std::string command_name;
		// Transaction ID is 1 for call connect responses
		double transaction_id;
		// Name-value pairs that describe the properties(fmsver etc.) of the connection.
		// @remark, never be NULL.
		RtmpAmf0Object* props;
		// Name-value pairs that describe the response from|the server. 'code',
		// 'level', 'description' are names of few among such information.
		// @remark, never be NULL.
		RtmpAmf0Object* info;

	public:
		RtmpConnectAppResPacket();
		virtual ~RtmpConnectAppResPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 4.1.2. Call
	// The call method of the NetConnection object runs remote procedure
	// calls (RPC) at the receiving end. The called RPC name is passed as a
	// parameter to the call command.
	class RtmpCallPacket : public RtmpPacket
	{
	public:
		// Name of the remote procedure that is called.
		std::string command_name;
		// If a response is expected we give a transaction Id. Else we pass a value of 0
		double transaction_id;
		// If there exists any command info this
		// is set, else this is set to null type.
		// @remark, optional, init to and maybe NULL.
		RtmpAmf0Any* command_object;
		// Any optional arguments to be provided
		// @remark, optional, init to and maybe NULL.
		RtmpAmf0Any* arguments;

	public:
		RtmpCallPacket();
		virtual ~RtmpCallPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};
	// Response  for RtmpCallPacket.
	class RtmpCallResPacket : public RtmpPacket
	{
	public:
		// Name of the command.
		std::string command_name;
		// ID of the command, to which the response belongs to
		double transaction_id;
		// If there exists any command info this is set, else this is set to null type.
		// @remark, optional, init to and maybe NULL.
		RtmpAmf0Any* command_object;
		// Response from the method that was called.
		// @remark, optional, init to and maybe NULL.
		RtmpAmf0Any* response;

	public:
		RtmpCallResPacket(double _transaction_id);
		virtual ~RtmpCallResPacket();
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 4.1.3. createStream
	// The client sends this command to the server to create a logical
	// channel for message communication The publishing of audio, video, and
	// metadata is carried out over stream channel created using the
	// createStream command.
	class RtmpCreateStreamPacket : public RtmpPacket
	{
	public:
		// Name of the command. Set to "createStream".
		std::string command_name;
		// Transaction ID of the command.
		double transaction_id;
		// If there exists any command info this is set, else this is set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
	public:
		RtmpCreateStreamPacket();
		virtual ~RtmpCreateStreamPacket();

	public:
		void set_command_object(RtmpAmf0Any* v);
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};
	// Response  for RtmpCreateStreamPacket.
	class RtmpCreateStreamResPacket : public RtmpPacket
	{
	public:
		// The _result or _error; indicates whether the response is result or error.
		std::string command_name;
		// ID of the command that response belongs to.
		double transaction_id;
		// If there exists any command info this is set, else this is set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// The return value is either a stream ID or an error information object.
		double stream_id;

	public:
		RtmpCreateStreamResPacket(double _transaction_id, double _stream_id);
		virtual ~RtmpCreateStreamResPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// client close stream packet.
	class RtmpCloseStreamPacket : public RtmpPacket
	{
	public:
		// Name of the command, set to "closeStream".
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information object does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
	public:
		RtmpCloseStreamPacket();
		virtual ~RtmpCloseStreamPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
	};

	// FMLE start publish: ReleaseStream/PublishStream/FCPublish/FCUnpublish
	class RtmpFMLEStartPacket : public RtmpPacket
	{
	public:
		// Name of the command
		std::string command_name;
		// The transaction ID to get the response.
		double transaction_id;
		// If there exists any command info this is set, else this is set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// The stream name to start publish or release.
		std::string stream_name;

	public:
		RtmpFMLEStartPacket();
		virtual ~RtmpFMLEStartPacket();

	public:
		void set_command_object(RtmpAmf0Any* v);
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
		// Factory method to create specified FMLE packet.
	public:
		static RtmpFMLEStartPacket* create_release_stream(std::string stream);
		static RtmpFMLEStartPacket* create_FC_publish(std::string stream);
	};
	// Response  for RtmpFMLEStartPacket.
	class RtmpFMLEStartResPacket : public RtmpPacket
	{
	public:
		// Name of the command
		std::string command_name;
		// The transaction ID to get the response.
		double transaction_id;
		// If there exists any command info this is set, else this is set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// The optional args, set to undefined.
		// @remark, never be NULL, an AMF0 undefined instance.
		RtmpAmf0Any* args; // undefined
	public:
		RtmpFMLEStartResPacket(double _transaction_id);
		virtual ~RtmpFMLEStartResPacket();

	public:
		void set_args(RtmpAmf0Any* v);
		void set_command_object(RtmpAmf0Any* v);
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// FMLE/flash publish
	// 4.2.6. Publish
	// The client sends the publish command to publish a named stream to the
	// server. Using this name, any client can play this stream and receive
	// The published audio, video, and data messages.
	class RtmpPublishPacket : public RtmpPacket
	{
	public:
		// Name of the command, set to "publish".
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information object does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// Name with which the stream is published.
		std::string stream_name;
		// Type of publishing. Set to "live", "record", or "append".
		//   record: The stream is published and the data is recorded to a new file.The file
		//           is stored on the server in a subdirectory within the directory that
		//           contains the server application. If the file already exists, it is
		//           overwritten.
		//   append: The stream is published and the data is appended to a file. If no file
		//           is found, it is created.
		//   live: Live data is published without recording it in a file.
		// @remark, SRS only support live.
		// @remark, optional, default to live.
		std::string type;

	public:
		RtmpPublishPacket();
		virtual ~RtmpPublishPacket();

	public:
		void set_command_object(RtmpAmf0Any* v);
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 4.2.8. pause
	// The client sends the pause command to tell the server to pause or
	// start playing.
	class RtmpPausePacket : public RtmpPacket
	{
	public:
		// Name of the command, set to "pause".
		std::string command_name;
		// There is no transaction ID for this command. Set to 0.
		double transaction_id;
		// Command information object does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// true or false, to indicate pausing or resuming play
		bool is_pause;
		// Number of milliseconds at which the the stream is paused or play resumed.
		// This is the current stream time at the Client when stream was paused. When the
		// playback is resumed, the server will only send messages with timestamps
		// greater than this value.
		double time_ms;

	public:
		RtmpPausePacket();
		virtual ~RtmpPausePacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
	};

	// 4.2.1. play
	// The client sends this command to the server to play a stream.
	class RtmpPlayPacket : public RtmpPacket
	{
	public:
		// Name of the command. Set to "play".
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// Name of the stream to play.
		// To play video (FLV) files, specify the name of the stream without a file
		//       extension (for example, "sample").
		// To play back MP3 or ID3 tags, you must precede the stream name with mp3:
		//       (for example, "mp3:sample".)
		// To play H.264/AAC files, you must precede the stream name with mp4: and specify the
		//       file extension. For example, to play the file sample.m4v, specify
		//       "mp4:sample.m4v"
		std::string stream_name;
		// An optional parameter that specifies the start time in seconds.
		// The default value is -2, which means the subscriber first tries to play the live
		//       stream specified in the Stream Name field. If a live stream of that name is
		//       not found, it plays the recorded stream specified in the Stream Name field.
		// If you pass -1 in the Start field, only the live stream specified in the Stream
		//       Name field is played.
		// If you pass 0 or a positive number in the Start field, a recorded stream specified
		//       in the Stream Name field is played beginning from the time specified in the
		//       Start field.
		// If no recorded stream is found, the next item in the playlist is played.
		double start;
		// An optional parameter that specifies the duration of playback in seconds.
		// The default value is -1. The -1 value means a live stream is played until it is no
		//       longer available or a recorded stream is played until it ends.
		// If u pass 0, it plays the single frame since the time specified in the Start field
		//       from the beginning of a recorded stream. It is assumed that the value specified
		//       in the Start field is equal to or greater than 0.
		// If you pass a positive number, it plays a live stream for the time period specified
		//       in the Duration field. After that it becomes available or plays a recorded
		//       stream for the time specified in the Duration field. (If a stream ends before the
		//       time specified in the Duration field, playback ends when the stream ends.)
		// If you pass a negative number other than -1 in the Duration field, it interprets the
		//       value as if it were -1.
		double duration;
		// An optional Boolean value or number that specifies whether to flush any
		// previous playlist.
		bool reset;

	public:
		RtmpPlayPacket();
		virtual ~RtmpPlayPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// Response  for RtmpPlayPacket.
	// @remark, user must set the stream_id in header.
	class RtmpPlayResPacket : public RtmpPacket
	{
	public:
		// Name of the command. If the play command is successful, the command
		// name is set to onStatus.
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* command_object; // null
		// If the play command is successful, the client receives OnStatus message from
		// server which is NetStream.Play.Start. If the specified stream is not found,
		// NetStream.Play.StreamNotFound is received.
		// @remark, never be NULL, an AMF0 object instance.
		RtmpAmf0Object* desc;

	public:
		RtmpPlayResPacket();
		virtual ~RtmpPlayResPacket();

	public:
		void set_command_object(RtmpAmf0Any* v);
		void set_desc(RtmpAmf0Object* v);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// When bandwidth test done, notice client.
	class RtmpOnBWDonePacket : public RtmpPacket
	{
	public:
		// Name of command. Set to "onBWDone"
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* args; // null
	public:
		RtmpOnBWDonePacket();
		virtual ~RtmpOnBWDonePacket();

	public:
		void set_args(RtmpAmf0Any* v);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// onStatus command, AMF0 Call
	// @remark, user must set the stream_id by RtmpCommonMessage.set_packet().
	class RtmpOnStatusCallPacket : public RtmpPacket
	{
	public:
		// Name of command. Set to "onStatus"
		std::string command_name;
		// Transaction ID set to 0.
		double transaction_id;
		// Command information does not exist. Set to null type.
		// @remark, never be NULL, an AMF0 null instance.
		RtmpAmf0Any* args; // null
		// Name-value pairs that describe the response from the server.
		// 'code','level', 'description' are names of few among such information.
		// @remark, never be NULL, an AMF0 object instance.
		RtmpAmf0Object* data;

	public:
		RtmpOnStatusCallPacket();
		virtual ~RtmpOnStatusCallPacket();

	public:
		void set_args(RtmpAmf0Any* v);
		void set_data(RtmpAmf0Object* v);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// onStatus data, AMF0 Data
	// @remark, user must set the stream_id by RtmpCommonMessage.set_packet().
	class RtmpOnStatusDataPacket : public RtmpPacket
	{
	public:
		// Name of command. Set to "onStatus"
		std::string command_name;
		// Name-value pairs that describe the response from the server.
		// 'code', are names of few among such information.
		// @remark, never be NULL, an AMF0 object instance.
		RtmpAmf0Object* data;

	public:
		RtmpOnStatusDataPacket();
		virtual ~RtmpOnStatusDataPacket();

	public:
		void set_data(RtmpAmf0Object* v);
		RtmpAmf0Object* get_data();
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// AMF0Data RtmpSampleAccess
	// @remark, user must set the stream_id by RtmpCommonMessage.set_packet().
	class RtmpSampleAccessPacket : public RtmpPacket
	{
	public:
		// Name of command. Set to "|RtmpSampleAccess".
		std::string command_name;
		// Whether allow access the sample of video.
		// @see:
		// http://help.adobe.com/en_US/FlashPlatform/reference/actionscript/3/flash/net/NetStream.html#videoSampleAccess
		bool video_sample_access;
		// Whether allow access the sample of audio.
		// @see:
		// http://help.adobe.com/en_US/FlashPlatform/reference/actionscript/3/flash/net/NetStream.html#audioSampleAccess
		bool audio_sample_access;

	public:
		RtmpSampleAccessPacket();
		virtual ~RtmpSampleAccessPacket();
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// The stream metadata.
	// FMLE: @setDataFrame
	// others: onMetaData
	class RtmpOnMetaDataPacket : public RtmpPacket
	{
	public:
		// Name of metadata. Set to "onMetaData"
		std::string name;
		// Metadata of stream.
		// @remark, never be NULL, an AMF0 object instance.
		RtmpAmf0Object* metadata;

	public:
		RtmpOnMetaDataPacket();
		virtual ~RtmpOnMetaDataPacket();

	public:
		void set_metadata(RtmpAmf0Object* v);
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 5.5. Window Acknowledgement Size (5)
	// The client or the server sends this message to inform the peer which
	// window size to use when sending acknowledgment.
	class RtmpSetWindowAckSizePacket : public RtmpPacket
	{
	public:
		int32_t ackowledgement_window_size;

	public:
		RtmpSetWindowAckSizePacket();
		virtual ~RtmpSetWindowAckSizePacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 5.3. Acknowledgement (3)
	// The client or the server sends the acknowledgment to the peer after
	// receiving bytes equal to the window size.
	class RtmpAcknowledgementPacket : public RtmpPacket
	{
	public:
		uint32_t sequence_number;

	public:
		RtmpAcknowledgementPacket();
		virtual ~RtmpAcknowledgementPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 7.1. Set Chunk Size
	// Protocol control message 1, Set Chunk Size, is used to notify the
	// peer about the new maximum chunk size.
	class RtmpSetChunkSizePacket : public RtmpPacket
	{
	public:
		// The maximum chunk size can be 65536 bytes. The chunk size is
		// maintained independently for each direction.
		int32_t chunk_size;

	public:
		RtmpSetChunkSizePacket();
		virtual ~RtmpSetChunkSizePacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 5.6. Set Peer Bandwidth (6)
	enum RtmpPeerBandwidthType
	{
		// The sender can mark this message hard (0), soft (1), or dynamic (2)
		// using the Limit type field.
		RtmpPeerBandwidthHard    = 0,
		RtmpPeerBandwidthSoft    = 1,
		RtmpPeerBandwidthDynamic = 2,
	};

	// 5.6. Set Peer Bandwidth (6)
	// The client or the server sends this message to update the output
	// bandwidth of the peer.
	class RtmpSetPeerBandwidthPacket : public RtmpPacket
	{
	public:
		int32_t bandwidth;
		// @see: RtmpPeerBandwidthType
		int8_t type;

	public:
		RtmpSetPeerBandwidthPacket();
		virtual ~RtmpSetPeerBandwidthPacket();
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

	// 3.7. User Control message
	enum SrcPCUCEventType
	{
		// Generally, 4bytes event-data

		// The server sends this event to notify the client
		// that a stream has become functional and can be
		// used for communication. By default, this event
		// is sent on ID 0 after the application connect
		// command is successfully received from the
		// client. The event data is 4-byte and represents
		// The stream ID of the stream that became
		// Functional.
		SrcPCUCStreamBegin = 0x00,

		// The server sends this event to notify the client
		// that the playback of data is over as requested
		// on this stream. No more data is sent without
		// issuing additional commands. The client discards
		// The messages received for the stream. The
		// 4 bytes of event data represent the ID of the
		// stream on which playback has ended.
		SrcPCUCStreamEOF = 0x01,

		// The server sends this event to notify the client
		// that there is no more data on the stream. If the
		// server does not detect any message for a time
		// period, it can notify the subscribed clients
		// that the stream is dry. The 4 bytes of event
		// data represent the stream ID of the dry stream.
		SrcPCUCStreamDry = 0x02,

		// The client sends this event to inform the server
		// of the buffer size (in milliseconds) that is
		// used to buffer any data coming over a stream.
		// This event is sent before the server starts
		// processing the stream. The first 4 bytes of the
		// event data represent the stream ID and the next
		// 4 bytes represent the buffer length, in
		// milliseconds.
		SrcPCUCSetBufferLength = 0x03, // 8bytes event-data

		// The server sends this event to notify the client
		// that the stream is a recorded stream. The
		// 4 bytes event data represent the stream ID of
		// The recorded stream.
		SrcPCUCStreamIsRecorded = 0x04,

		// The server sends this event to test whether the
		// client is reachable. Event data is a 4-byte
		// timestamp, representing the local server time
		// When the server dispatched the command. The
		// client responds with kMsgPingResponse on
		// receiving kMsgPingRequest.
		SrcPCUCPingRequest = 0x06,

		// The client sends this event to the server in
		// Response  to the ping request. The event data is
		// a 4-byte timestamp, which was received with the
		// kMsgPingRequest request.
		SrcPCUCPingResponse = 0x07,

		// For PCUC size=3, for example the payload is "00 1A 01",
		// it's a FMS control event, where the event type is 0x001a and event data is 0x01,
		// please notice that the event data is only 1 byte for this event.
		RtmpPCUCFmsEvent0 = 0x1a,
	};

	// 5.4. User Control Message (4)
	//
	// For the EventData is 4bytes.
	// Stream Begin(=0)              4-bytes stream ID
	// Stream EOF(=1)                4-bytes stream ID
	// StreamDry(=2)                 4-bytes stream ID
	// SetBufferLength(=3)           8-bytes 4bytes stream ID, 4bytes buffer length.
	// StreamIsRecorded(=4)          4-bytes stream ID
	// PingRequest(=6)               4-bytes timestamp local server time
	// PingResponse(=7)              4-bytes timestamp received ping request.
	//
	// 3.7. User Control message
	// +------------------------------+-------------------------
	// | Event Type ( 2- bytes ) | Event Data
	// +------------------------------+-------------------------
	// Figure 5 Pay load for the 'User Control Message'.
	class RtmpUserControlPacket : public RtmpPacket
	{
	public:
		// Event type is followed by Event data.
		// @see: SrcPCUCEventType
		int16_t event_type;
		// The event data generally in 4bytes.
		// @remark for event type is 0x001a, only 1bytes.
		// @see RtmpPCUCFmsEvent0
		int32_t event_data;
		// 4bytes if event_type is SetBufferLength; otherwise 0.
		int32_t extra_data;

	public:
		RtmpUserControlPacket();
		virtual ~RtmpUserControlPacket();
		// Decode functions for concrete packet to override.
	public:
		virtual srs_error_t decode(Utils::RtmpBuffer* stream);
		// Encode functions for concrete packet to override.
	public:
		virtual int get_prefer_cid();
		virtual int get_message_type();

	protected:
		virtual int get_size();
		virtual srs_error_t encode_packet(Utils::RtmpBuffer* stream);
	};

} // namespace RTMP

#endif
