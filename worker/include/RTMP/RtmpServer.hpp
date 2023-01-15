



/**
 * Rtmp Server 用于listen Rtmp tcp connection，并创建对应的RtmpTransport
 * 由worker在nodejs请求后创建出RtmpServer. config由nodejs提供。
 * 
*/

#ifndef MS_RTMP_SERVER_HPP
#define MS_RTMP_SERVER_HPP

namespace RTMP
{
	class RtmpServer {
        /**
         * connManager : 管理rtmp 链接，一个链接对应一个streamUrl以及链路. RtmpTransportManager
        */
	};
} // namespace RTMP
#endif