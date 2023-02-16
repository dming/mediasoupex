/**
 * how many chunk stream to cache, [0, N].
 * to imporove about 10% performance when chunk size small, and 5% for large chunk.
 * @see https://github.com/ossrs/srs/issues/249
 * @remark 0 to disable the chunk stream cache.
 */
#define SRS_PERF_CHUNK_STREAM_CACHE 16

// The default chunk size for system.
#define SRS_CONSTS_RTMP_SRS_CHUNK_SIZE 60000
// 6. Chunking, RTMP protocol default chunk size.
#define SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE 128