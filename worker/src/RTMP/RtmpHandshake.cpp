#define MS_CLASS "RTMP::RtmpHandshake"
#define MS_LOG_DEV_LEVEL 3

#include "RTMP/RtmpHandshake.hpp"
#include "Logger.hpp"
#include "RTMP/RtmpTcpConnection.hpp"
#include <random>

namespace RTMP
{
	RtmpHandshakeBytes::RtmpHandshakeBytes()
	  : c0c1(nullptr), s0s1s2(nullptr), c2(nullptr), done(false)
	{
		MS_TRACE();
	}

	RtmpHandshakeBytes::~RtmpHandshakeBytes()
	{
		MS_TRACE();
	}

	void RtmpHandshakeBytes::dispose()
	{
		MS_TRACE();
		FREEPA(c0c1);
		FREEPA(s0s1s2);
		FREEPA(c2);
	}

	int RtmpHandshakeBytes::readC0c1(RtmpTcpConnection* io)
	{
		int err = 0;
		if (c0c1)
			return 0;

		c0c1 = new char[1537];
		if (err = io->ReadFully(c0c1, 1537) != 0)
		{
			MS_ERROR("readC0c1 fail");
			FREEP(c0c1);
			return err;
		}
		return 0;
	}

	int RtmpHandshakeBytes::readS0s1s2(RtmpTcpConnection* io)
	{
		int err = 0;
		if (s0s1s2)
			return 0;

		s0s1s2 = new char[3073];
		if (err = io->ReadFully(s0s1s2, 3073) != 0)
		{
			MS_ERROR("readS0s1s2 fail");
			FREEP(s0s1s2);
			return err;
		}
		return 0;
	}

	int RtmpHandshakeBytes::readC2(RtmpTcpConnection* io)
	{
		MS_TRACE();
		int err = 0;
		if (c2)
			return 0;

		c2 = new char[1536];
		if (err = io->ReadFully(c2, 1536) != 0)
		{
			MS_ERROR("readC2 fail");
			FREEP(c2);
			return err;
		}
		return 0;
	}

	int RtmpHandshakeBytes::createC0c1()
	{
		if (c0c1)
			return 0;

		c0c1 = new char[1537];
		memset(c0c1, 0, 1537);
		c0c1[0] = RTMP_VERSION;

		std::random_device rd;
		char* p = c0c1;
		p += 9;
		for (int i = 0; i < 1528; i++)
		{
			*p++ = rd();
		}
		return 0;
	}

	int RtmpHandshakeBytes::createS0s1s2(const char* c1)
	{
		if (s0s1s2)
			return 0;

		s0s1s2 = new char[3073];
		memset(s0s1s2, 0, 3073);
		s0s1s2[0] = RTMP_VERSION;

		std::random_device rd;
		char* p = s0s1s2;
		p += 9;
		for (int i = 0; i < 3064; i++)
		{
			*p++ = rd();
		}

		// if c1 specified, copy c1 to s2.
		// @see: https://github.com/ossrs/srs/issues/46
		if (c1)
		{
			memcpy(s0s1s2 + 1537, c1, 1536);
		}
		return 0;
	}

	int RtmpHandshakeBytes::createC2()
	{
		if (c2)
			return 0;

		c2 = new char[1536];
		memset(c2, 0, 1536);

		std::random_device rd;
		for (int i = 0; i < 1536; i++)
		{
			*c2++ = rd();
		}

		return 0;
	}

	RtmpHandshake::RtmpHandshake()
	{
		MS_TRACE();
	}

	RtmpHandshake::~RtmpHandshake()
	{
		MS_TRACE();
	}

	int RtmpHandshake::HandshakeWithClient(RtmpHandshakeBytes* hsBytes, RtmpTcpConnection* connection)
	{
		MS_TRACE();
		if (hsBytes->readC0c1(connection) != 0)
		{
			MS_ERROR("read c0c1 fail");
			return -1;
		}

		if (hsBytes->c0c1[0] != 0x03)
		{
			MS_ERROR("only support rtmp plain text, version=%" PRIu8, (uint8_t)hsBytes->c0c1[0]);
			return -1;
		}

		if (!hsBytes->s0s1s2 && hsBytes->createS0s1s2() != 0)
		{
			MS_ERROR("create s0s1s2 fail");
			return -1;
		}

		if (!hsBytes->sendS0s1s2)
		{
			connection->Send((const uint8_t*)hsBytes->s0s1s2, 3073, nullptr);
			hsBytes->sendS0s1s2 = true;
			return 0;
		}

		if (hsBytes->readC2(connection) != 0)
		{
			MS_ERROR("read c2 fail");
			return -1;
		}

		hsBytes->done = true;
		MS_DEBUG_DEV(
		  "RtmpHandshake::HandshakeWithClient Done. \n Peer:%s:%d",
		  connection->GetPeerIp().c_str(),
		  connection->GetPeerPort());
		return 0;
	}
} // namespace RTMP