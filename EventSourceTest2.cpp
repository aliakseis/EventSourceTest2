// EventSourceTest2.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "http.h"

#include <iostream>
#include <string>
#include <thread>
#include <atomic>

static size_t on_data(char *ptr, size_t size, size_t nmemb/*, void *userdata*/)
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

using namespace std::chrono_literals;

int main(int argc, char** argv)
{
    options.arg0 = *argv;

    const char url[] = "https://ntfy.sh/eventSourceExample/sse";

    static const char* headers[] = {
        "Accept: text/event-stream",
        NULL
    };

    std::thread th([url] {
        http(HTTP_GET, url, headers, 0, 0, on_data, verify_sse_response, progress_callback);
        });

    std::this_thread::sleep_for(2000ms);

    const char message[] = "Hi there!!!";

    http(HTTP_POST, "https://ntfy.sh/eventSourceExample", nullptr, message, sizeof(message) - 1);

    std::this_thread::sleep_for(2000ms);

    requestInterrupted = true;

    th.join();

    return 0;
}
