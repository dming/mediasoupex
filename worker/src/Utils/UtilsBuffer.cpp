#define MS_CLASS "Utils::Buffer"
// #define MS_LOG_DEV_LEVEL 3

#include "UtilsBuffer.hpp"
#include "CplxError.hpp"

namespace Utils
{

	RtmpBuffer::RtmpBuffer(char* b, int nn)
	{
		p = bytes = b;
		nb_bytes  = nn;
	}

	RtmpBuffer::~RtmpBuffer()
	{
	}

	RtmpBuffer* RtmpBuffer::copy()
	{
		RtmpBuffer* cp = new RtmpBuffer(bytes, nb_bytes);
		cp->p          = p;
		return cp;
	}

	char* RtmpBuffer::data()
	{
		return bytes;
	}

	char* RtmpBuffer::head()
	{
		return p;
	}

	int RtmpBuffer::size()
	{
		return nb_bytes;
	}

	void RtmpBuffer::set_size(int v)
	{
		nb_bytes = v;
	}

	int RtmpBuffer::pos()
	{
		return (int)(p - bytes);
	}

	int RtmpBuffer::left()
	{
		return nb_bytes - (int)(p - bytes);
	}

	bool RtmpBuffer::empty()
	{
		return !bytes || (p >= bytes + nb_bytes);
	}

	bool RtmpBuffer::require(int required_size)
	{
		if (required_size < 0)
		{
			return false;
		}

		return required_size <= nb_bytes - (p - bytes);
	}

	void RtmpBuffer::skip(int size)
	{
		srs_assert(p);
		srs_assert(p + size >= bytes);
		srs_assert(p + size <= bytes + nb_bytes);

		p += size;
	}

	int8_t RtmpBuffer::read_1bytes()
	{
		srs_assert(require(1));

		return (int8_t)*p++;
	}

	int16_t RtmpBuffer::read_2bytes()
	{
		srs_assert(require(2));

		int16_t value;
		char* pp = (char*)&value;
		pp[1]    = *p++;
		pp[0]    = *p++;

		return value;
	}

	int16_t RtmpBuffer::read_le2bytes()
	{
		srs_assert(require(2));

		int16_t value;
		char* pp = (char*)&value;
		pp[0]    = *p++;
		pp[1]    = *p++;

		return value;
	}

	int32_t RtmpBuffer::read_3bytes()
	{
		srs_assert(require(3));

		int32_t value = 0x00;
		char* pp      = (char*)&value;
		pp[2]         = *p++;
		pp[1]         = *p++;
		pp[0]         = *p++;

		return value;
	}

	int32_t RtmpBuffer::read_le3bytes()
	{
		srs_assert(require(3));

		int32_t value = 0x00;
		char* pp      = (char*)&value;
		pp[0]         = *p++;
		pp[1]         = *p++;
		pp[2]         = *p++;

		return value;
	}

	int32_t RtmpBuffer::read_4bytes()
	{
		srs_assert(require(4));

		int32_t value;
		char* pp = (char*)&value;
		pp[3]    = *p++;
		pp[2]    = *p++;
		pp[1]    = *p++;
		pp[0]    = *p++;

		return value;
	}

	int32_t RtmpBuffer::read_le4bytes()
	{
		srs_assert(require(4));

		int32_t value;
		char* pp = (char*)&value;
		pp[0]    = *p++;
		pp[1]    = *p++;
		pp[2]    = *p++;
		pp[3]    = *p++;

		return value;
	}

	int64_t RtmpBuffer::read_8bytes()
	{
		srs_assert(require(8));

		int64_t value;
		char* pp = (char*)&value;
		pp[7]    = *p++;
		pp[6]    = *p++;
		pp[5]    = *p++;
		pp[4]    = *p++;
		pp[3]    = *p++;
		pp[2]    = *p++;
		pp[1]    = *p++;
		pp[0]    = *p++;

		return value;
	}

	int64_t RtmpBuffer::read_le8bytes()
	{
		srs_assert(require(8));

		int64_t value;
		char* pp = (char*)&value;
		pp[0]    = *p++;
		pp[1]    = *p++;
		pp[2]    = *p++;
		pp[3]    = *p++;
		pp[4]    = *p++;
		pp[5]    = *p++;
		pp[6]    = *p++;
		pp[7]    = *p++;

		return value;
	}

	std::string RtmpBuffer::read_string(int len)
	{
		srs_assert(require(len));

		std::string value;
		value.append(p, len);

		p += len;

		return value;
	}

	void RtmpBuffer::read_bytes(char* data, int size)
	{
		srs_assert(require(size));

		memcpy(data, p, size);

		p += size;
	}

	void RtmpBuffer::write_1bytes(int8_t value)
	{
		srs_assert(require(1));

		*p++ = value;
	}

	void RtmpBuffer::write_2bytes(int16_t value)
	{
		srs_assert(require(2));

		char* pp = (char*)&value;
		*p++     = pp[1];
		*p++     = pp[0];
	}

	void RtmpBuffer::write_le2bytes(int16_t value)
	{
		srs_assert(require(2));

		char* pp = (char*)&value;
		*p++     = pp[0];
		*p++     = pp[1];
	}

	void RtmpBuffer::write_4bytes(int32_t value)
	{
		srs_assert(require(4));

		char* pp = (char*)&value;
		*p++     = pp[3];
		*p++     = pp[2];
		*p++     = pp[1];
		*p++     = pp[0];
	}

	void RtmpBuffer::write_le4bytes(int32_t value)
	{
		srs_assert(require(4));

		char* pp = (char*)&value;
		*p++     = pp[0];
		*p++     = pp[1];
		*p++     = pp[2];
		*p++     = pp[3];
	}

	void RtmpBuffer::write_3bytes(int32_t value)
	{
		srs_assert(require(3));

		char* pp = (char*)&value;
		*p++     = pp[2];
		*p++     = pp[1];
		*p++     = pp[0];
	}

	void RtmpBuffer::write_le3bytes(int32_t value)
	{
		srs_assert(require(3));

		char* pp = (char*)&value;
		*p++     = pp[0];
		*p++     = pp[1];
		*p++     = pp[2];
	}

	void RtmpBuffer::write_8bytes(int64_t value)
	{
		srs_assert(require(8));

		char* pp = (char*)&value;
		*p++     = pp[7];
		*p++     = pp[6];
		*p++     = pp[5];
		*p++     = pp[4];
		*p++     = pp[3];
		*p++     = pp[2];
		*p++     = pp[1];
		*p++     = pp[0];
	}

	void RtmpBuffer::write_le8bytes(int64_t value)
	{
		srs_assert(require(8));

		char* pp = (char*)&value;
		*p++     = pp[0];
		*p++     = pp[1];
		*p++     = pp[2];
		*p++     = pp[3];
		*p++     = pp[4];
		*p++     = pp[5];
		*p++     = pp[6];
		*p++     = pp[7];
	}

	void RtmpBuffer::write_string(std::string value)
	{
		if (value.empty())
		{
			return;
		}

		srs_assert(require((int)value.length()));

		memcpy(p, value.data(), value.length());
		p += value.length();
	}

	void RtmpBuffer::write_bytes(char* data, int size)
	{
		if (size <= 0)
		{
			return;
		}

		srs_assert(require(size));

		memcpy(p, data, size);
		p += size;
	}

} // namespace Utils
