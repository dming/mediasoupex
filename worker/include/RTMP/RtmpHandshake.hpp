#ifndef MS_RTMP_HANDSHAKE_HPP
#define MS_RTMP_HANDSHAKE_HPP

#include <stdint.h>

namespace RTMP
{
	class RtmpTcpConnection;

	class RtmpHandshakeBytes
	{
	public:
		static const int RTMP_VERSION = 0x3;

	public:
		// [1+1536]
		char* c0c1;
		// [1+1536+1536]
		char* s0s1s2;
		// [1536]
		char* c2;

		bool sendC0c1{ false };
		bool sendS0s1s2{ false };
		bool sendC2{ false };
		bool done{ false };

	public:
		RtmpHandshakeBytes();
		virtual ~RtmpHandshakeBytes();

	public:
		virtual void dispose();

	public:
		virtual int readC0c1(RtmpTcpConnection* io);
		virtual int readS0s1s2(RtmpTcpConnection* io);
		virtual int readC2(RtmpTcpConnection* io);
		virtual int createC0c1();
		virtual int createS0s1s2(const char* c1 = NULL);
		virtual int createC2();
	};
	class RtmpHandshake
	{
	public:
		RtmpHandshake();
		~RtmpHandshake();

		// int Parse(uint8_t* data, size_t dataLen, char* res, size_t resLen);
		int HandshakeWithClient(RtmpHandshakeBytes* hsBytes, RtmpTcpConnection* connection);
	};
} // namespace RTMP

#endif