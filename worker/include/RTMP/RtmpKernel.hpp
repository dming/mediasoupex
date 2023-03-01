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

#endif