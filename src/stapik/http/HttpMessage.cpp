#include "HttpMessage.hpp"

namespace stapik::http
{
    Bytes toBytes(const std::string_view text)
    {
        return { text.begin(), text.end() };
    }

    std::string toText(const Bytes& bytes)
    {
        return { bytes.begin(), bytes.end() };
    }
}
