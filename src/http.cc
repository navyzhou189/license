
#include "http.h"


#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // must be included if log user defined object
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

HttpClient::HttpClient() {
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

HttpClient::~HttpClient() {
    curl_global_cleanup();
}

int HttpClient::Get(const std::string& req, const std::string& resp) {
    std::lock_guard<std::mutex> lk(mtx);
    if (!connIsOpened()) {
        int ret = openConn();
        if (ret != EHTTP_OK) {
            return ret;
        }
    }

    CURLcode res;
    curl_easy_setopt(handle.curl, CURLOPT_URL, "http://192.168.11.25:6000/api/v2/vcloud/license/getallauthinfo");
    curl_easy_setopt(handle.curl, CURLOPT_HTTPGET, 1L);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    curl_easy_setopt(handle.curl, CURLOPT_FOLLOWLOCATION, 1L); // tell us to follow redirection if redirected is need
    res = curl_easy_perform(handle.curl);
    if(res != CURLE_OK) {
        SPDLOG_ERROR("curl_easy_perform() failed: {0}",curl_easy_strerror(res));
    }
    
    
}

int HttpClient::Put(const std::string& req, const std::string& resp) {
    std::lock_guard<std::mutex> lk(mtx);
}

int HttpClient::Post(const std::string& req, const std::string& resp) {
    std::lock_guard<std::mutex> lk(mtx);
}

bool HttpClient::connIsOpened() {
    return connOpened;
}

int HttpClient::openConn() {
    handle.curl = curl_easy_init();
    if(!(handle.curl)) {
        SPDLOG_ERROR("curl_easy_init() failed, the connection to remote is not build");
        return EHTTP_OPEN_CONN_FAILURE;
    }

    handle.curl = nullptr;
    connOpened = true;
    return EHTTP_OK;
}

void HttpClient::closeConn() {
    if (handle.curl) {
        curl_easy_cleanup(handle.curl);
    }
    
    handle.curl = nullptr;
    connOpened = false;
}