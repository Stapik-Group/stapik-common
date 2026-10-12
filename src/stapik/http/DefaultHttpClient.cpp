#include "CurlHttpClient.hpp"

#include "stapik/app/AppContext.hpp"
#include "stapik/app/UserAgent.hpp"

namespace stapik::http
{
    namespace
    {
        std::string applicationUserAgent()
        {
            if (!app::AppContext::isInitialized())
                return app::userAgentOf({});

            return app::userAgentOf(app::AppContext::instance().info());
        }
    }

    const IHttpClient &defaultHttpClient()
    {
        static const CurlHttpClient client(applicationUserAgent);
        return client;
    }
}
