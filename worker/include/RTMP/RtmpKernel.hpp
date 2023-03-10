#ifndef MS_RTMP_KERNEL_HPP
#define MS_RTMP_KERNEL_HPP

#include <stdint.h>

// Default port of rtmp
#define SRS_CONSTS_RTMP_DEFAULT_PORT 1935

// Default http listen port.
#define SRS_DEFAULT_HTTP_PORT 80

// Default https listen port.
#define SRS_DEFAULT_HTTPS_PORT 443

// Default Redis listen port.
#define SRS_DEFAULT_REDIS_PORT 6379

typedef int64_t srs_utime_t;

///////////////////////////////////////////////////////////
// Default vhost of rtmp
#define SRS_CONSTS_RTMP_DEFAULT_VHOST "__defaultVhost__"
#define SRS_CONSTS_RTMP_DEFAULT_APP "__defaultApp__"

// The default chunk size for system.
#define SRS_CONSTS_RTMP_SRS_CHUNK_SIZE 60000
// 6. Chunking, RTMP protocol default chunk size.
#define SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE 128

// 6. Chunking
// The chunk size is configurable. It can be set using a control
// message(Set Chunk Size) as described in section 7.1. The maximum
// chunk size can be 65536 bytes and minimum 128 bytes. Larger values
// reduce CPU usage, but also commit to larger writes that can delay
// other content on lower bandwidth connections. Smaller chunks are not
// good for high-bit rate streaming. Chunk size is maintained
// independently for each direction.
#define SRS_CONSTS_RTMP_MIN_CHUNK_SIZE 128
#define SRS_CONSTS_RTMP_MAX_CHUNK_SIZE 65536

// To convert macro values to string.
// @see https://gcc.gnu.org/onlinedocs/cpp/Stringification.html#Stringification
#define SRS_INTERNAL_STR(v) #v
#define SRS_XSTR(v) SRS_INTERNAL_STR(v)

// The project informations, may sent to client in HTTP header or RTMP metadata.
#define RTMP_SIG_SRS_KEY "SRS"
#define RTMP_SIG_SRS_CODE "Bee"
#define RTMP_SIG_SRS_URL "https://github.com/ossrs/srs"
#define RTMP_SIG_SRS_LICENSE "MIT"
#define SRS_CONSTRIBUTORS "https://github.com/ossrs/srs/blob/develop/trunk/AUTHORS.md#contributors"
#define RTMP_SIG_SRS_VERSION                                                                       \
	SRS_XSTR(VERSION_MAJOR) "." SRS_XSTR(VERSION_MINOR) "." SRS_XSTR(VERSION_REVISION)
#define RTMP_SIG_SRS_SERVER RTMP_SIG_SRS_KEY "/" RTMP_SIG_SRS_VERSION "(" RTMP_SIG_SRS_CODE ")"
#define RTMP_SIG_SRS_DOMAIN "ossrs.net"
#define RTMP_SIG_SRS_AUTHORS "srs"

// The max rtmp header size:
//     1bytes basic header,
//     11bytes message header,
//     4bytes timestamp header,
// that is, 1+11+4=16bytes.
#define SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE 16
// The max rtmp header size:
//     1bytes basic header,
//     4bytes timestamp header,
// that is, 1+4=5bytes.
// always use fmt0 as cache.
#define SRS_CONSTS_RTMP_MAX_FMT3_HEADER_SIZE 5

/**
 * how many msgs can be send entirely.
 * for play clients to get msgs then totally send out.
 * for the mw sleep set to 1800, the msgs is about 133.
 * @remark, recomment to 128.
 */
#define SRS_PERF_MW_MSGS 128
// For performance issue,
// iovs cache for multiple messages for each connections.
// suppose the chunk size is 64k, each message send in a chunk which needs only 2 iovec,
// so the iovs max should be (SRS_PERF_MW_MSGS * 2)
//
// @remark, SRS will realloc when the iovs not enough.
#define SRS_CONSTS_IOVS_MAX (SRS_PERF_MW_MSGS * 2)
// For performance issue,
// c0c3 cache for multiple messages for each connections.
// each c0 <= 16byes, suppose the chunk size is 64k,
// each message send in a chunk which needs only a c0 header,
// so the c0c3 cache should be (SRS_PERF_MW_MSGS * 16)
//
// @remark, SRS will try another loop when c0c3 cache dry, for we cannot realloc it.
//       so we use larger c0c3 cache, that is (SRS_PERF_MW_MSGS * 32)
#define SRS_CONSTS_C0C3_HEADERS_MAX (SRS_PERF_MW_MSGS * 32)

#define RtmpAutoFree(className, instance)                                                          \
	std::unique_ptr<className> _auto_free_##instance(instance);

#define SRS_UTIME_MILLISECONDS 1000

// H.264 nalu header type mask.
const uint8_t kNalTypeMask = 0x1F;
#endif