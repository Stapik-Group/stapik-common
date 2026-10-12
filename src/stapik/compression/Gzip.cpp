#include "Gzip.hpp"

#include <zlib.h>

#include <array>
#include <limits>

namespace stapik::compression
{
    namespace
    {
        constexpr int GZIP_WINDOW_BITS = MAX_WBITS + 16;
        constexpr int DEFAULT_MEMORY_LEVEL = 8;
        constexpr std::size_t INFLATE_CHUNK_BYTES = 64 * 1024;

        bool fitsInZlibCounter(const std::size_t size)
        {
            return size <= std::numeric_limits<uInt>::max();
        }

        Bytef* mutableBytes(const std::string_view text)
        {
            return reinterpret_cast<Bytef*>(const_cast<char*>(text.data()));
        }

        class InflateStream
        {
        public:
            InflateStream() :
                m_initialized(inflateInit2(&m_stream, GZIP_WINDOW_BITS) == Z_OK)
            {}

            ~InflateStream()
            {
                if (m_initialized)
                    inflateEnd(&m_stream);
            }

            InflateStream(const InflateStream&) = delete;
            InflateStream& operator=(const InflateStream&) = delete;

            [[nodiscard]] bool isReady() const
            {
                return m_initialized;
            }

            [[nodiscard]] z_stream& raw()
            {
                return m_stream;
            }

        private:
            z_stream m_stream{};
            bool m_initialized;
        };
    }

    std::string_view describe(const GzipError error)
    {
        switch (error)
        {
            using enum GzipError;
            case CompressionFailed: return "the data could not be compressed";
            case Malformed: return "the compressed data is damaged";
            case TooLarge: return "the decompressed data is too large";
        }

        return "unknown error";
    }

    GzipResult gzipCompress(const std::string_view content)
    {
        if (!fitsInZlibCounter(content.size()))
            return Failure(GzipError::CompressionFailed);

        z_stream stream{};
        if (deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, GZIP_WINDOW_BITS, DEFAULT_MEMORY_LEVEL, Z_DEFAULT_STRATEGY) != Z_OK)
            return Failure(GzipError::CompressionFailed);

        std::string compressed(deflateBound(&stream, content.size()), '\0');
        stream.next_in = mutableBytes(content);
        stream.avail_in = static_cast<uInt>(content.size());
        stream.next_out = reinterpret_cast<Bytef*>(compressed.data());
        stream.avail_out = static_cast<uInt>(compressed.size());

        const int status = deflate(&stream, Z_FINISH);
        const auto compressedSize = stream.total_out;
        deflateEnd(&stream);

        if (status != Z_STREAM_END)
            return Failure(GzipError::CompressionFailed);

        compressed.resize(compressedSize);
        return compressed;
    }

    GzipResult gzipDecompress(const std::string_view compressed, const std::size_t maxDecompressedSize)
    {
        if (!fitsInZlibCounter(compressed.size()))
            return Failure(GzipError::Malformed);

        InflateStream inflater;
        if (!inflater.isReady())
            return Failure(GzipError::Malformed);

        auto& stream = inflater.raw();
        stream.next_in = mutableBytes(compressed);
        stream.avail_in = static_cast<uInt>(compressed.size());

        std::string decompressed;
        std::array<char, INFLATE_CHUNK_BYTES> chunk{};
        int status = Z_OK;

        while (status != Z_STREAM_END)
        {
            stream.next_out = reinterpret_cast<Bytef*>(chunk.data());
            stream.avail_out = static_cast<uInt>(chunk.size());

            status = inflate(&stream, Z_NO_FLUSH);
            if (status != Z_OK && status != Z_STREAM_END)
                return Failure(GzipError::Malformed);

            const std::size_t produced = chunk.size() - stream.avail_out;
            if (decompressed.size() + produced > maxDecompressedSize)
                return Failure(GzipError::TooLarge);

            decompressed.append(chunk.data(), produced);
        }

        return decompressed;
    }
}
