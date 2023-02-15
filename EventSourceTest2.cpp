// EventSourceTest2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "http.h"
#include "json_parser.h"

#include <iostream>
#include <string>
#include <thread>
#include <atomic>
#include <future>

/*
static size_t on_data(char *ptr, size_t size, size_t nmemb)
{
    //logger(2, ptr, size * nmemb, 0);
    //parse_sse(ptr, size * nmemb);

    std::string s(ptr, size * nmemb);

    std::cout << "*** Start of data ***\n" << s << "\n*** End of data ***" << std::endl;

    return size * nmemb;
}

static std::atomic_bool requestInterrupted = false;

static size_t progress_callback(//void *clientp,
    curl_off_t dltotal,
    curl_off_t dlnow,
    curl_off_t ultotal,
    curl_off_t ulnow)
{
    return requestInterrupted;
}
*/

static const char* verify_sse_response(CURL* curl) {
#define EXPECTED_CONTENT_TYPE "text/event-stream"

    static const char expected_content_type[] = EXPECTED_CONTENT_TYPE;

    const char* content_type;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &content_type);
    if (!content_type) content_type = "";

    if (!strncmp(content_type, expected_content_type, strlen(expected_content_type)))
        return 0;

    return "Invalid content_type, should be '" EXPECTED_CONTENT_TYPE "'.";
}



auto getRemoteEcho()
{
    std::promise<bool> startedPromise;
    std::promise<std::string> responsePromise;

    auto startedResult = startedPromise.get_future();
    auto responseResult = responsePromise.get_future();

    auto threadLam = [] (
        std::promise<bool> startedPromise,
        std::promise<std::string> responsePromise
    ) {

        const char url[] = "https://ntfy.sh/eventSourceExample/sse";

        const char* headers[] = {
            "Accept: text/event-stream",
            NULL
        };

        bool requestInterrupted = false;
        //int interruptedCount = 1;

        auto on_data = [&startedPromise, &responsePromise, &requestInterrupted](char *ptr, size_t size, size_t nmemb)->size_t {
            try {
                const auto ptrEnd = ptr + size * nmemb;

                const char watch[] = "data:";

                auto pData = std::search(ptr, ptrEnd, std::begin(watch), std::prev(std::end(watch)));
                if (pData != ptrEnd)
                {
                    pData += sizeof(watch) / sizeof(watch[0]) - 1;
                    std::string_view v(pData, ptrEnd - pData);
                    auto json = parseJson(v);
                    const auto& document = std::any_cast<const std::map<std::string, std::any>&>(json);
                    if (auto event = document.find("event"); event != document.end())
                    {
                        const auto& eventValue = std::any_cast<const std::string&>(event->second);
                        if (eventValue == "open")
                        {
                            startedPromise.set_value(true);
                        }
                        else if (eventValue == "message")
                        {
                            if (auto message = document.find("message"); message != document.end())
                            {
                                const auto& messageValue = std::any_cast<const std::string&>(message->second);
                                responsePromise.set_value(messageValue);
                                requestInterrupted = true;
                            }
                        }
                    }
                }
            }
            catch (const std::exception&) {
                startedPromise.set_value(false);
                requestInterrupted = true;
            }
            return size * nmemb;
        };

        auto progress_callback = [&requestInterrupted/*, &interruptedCount*/](curl_off_t dltotal,
            curl_off_t dlnow,
            curl_off_t ultotal,
            curl_off_t ulnow)->size_t {
                return requestInterrupted;
                //if (!requestInterrupted)
                //    return 0;
                //if (interruptedCount == 0)
                //    return 1;
                //--interruptedCount;
                //return 0;
        };


        http(HTTP_GET, url, headers, 0, 0, on_data, verify_sse_response, progress_callback);
    };

    // https://stackoverflow.com/a/23454840/10472202
    std::thread(threadLam, std::move(startedPromise), std::move(responsePromise)).detach();

    return std::make_tuple(std::move(startedResult), std::move(responseResult));
}



using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    options.arg0 = *argv;

    //const char url[] = "https://ntfy.sh/eventSourceExample/sse";

    //static const char* headers[] = {
    //    "Accept: text/event-stream",
    //    NULL
    //};

    //std::thread th([url] {
    //    http(HTTP_GET, url, headers, 0, 0, on_data, verify_sse_response, progress_callback);
    //    });

    //std::this_thread::sleep_for(2000ms);

    auto[startedResult, responseResult] = getRemoteEcho();

    if (!startedResult.get()) {
        std::cerr << "Failed to SSE connect to the server.\n";
        return 1;
    }

    const char message[] = "Hi there!!!\nHi there!!!";

    http(HTTP_POST, "https://ntfy.sh/eventSourceExample", nullptr, message, sizeof(message) - 1);


    auto response = responseResult.get();

    //std::this_thread::sleep_for(50ms);

    std::cout << "Response: " << response << std::endl;

    //std::this_thread::sleep_for(2000ms);

    //requestInterrupted = true;

    //th.join();

    return 0;
}
