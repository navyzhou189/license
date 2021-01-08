
#include "http.h"


#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // must be included if log user defined object
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

#include <stdlib.h>


static size_t doCurlWriteCB(void *data, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    HttpReply *mem = (HttpReply*)userp;

    char *ptr = (char*)realloc(mem->response, mem->size + realsize + 1);
    if(ptr == NULL)
        return 0;  /* out of memory! */

    mem->response = ptr;
    memcpy(&(mem->response[mem->size]), data, realsize);
    mem->size += realsize;
    mem->response[mem->size] = 0;

    return realsize;
}

HttpClient::HttpClient() {
    CURLcode ret = curl_global_init(CURL_GLOBAL_DEFAULT);
    if(ret != CURLE_OK) {
        SPDLOG_ERROR("curl_easy_perform() failed: {0}",curl_easy_strerror(ret));
        abort();
    }
    ppCurlHandle = new (CURL*);
    
}

HttpClient::~HttpClient() {

    if (ppCurlHandle) {
        delete ppCurlHandle;
        ppCurlHandle = nullptr;
    }
    curl_global_cleanup();
}


// bug to be fixed: cause the method is a syc-ping-pong, it will block indefinity when remote peer don't send a response.
int HttpClient::Get(const std::string& url, std::string& reply) {
    std::lock_guard<std::mutex> lk(execlusive_op_protect);

    // make sure connection is opened before use, if fail return error
    if (!connIsOpened()) {
        int ret = openConn();
        if (ret != EHTTP_OK) {
            return ret;
        }
    }

    CURLcode res;
    curl_easy_setopt(*ppCurlHandle, CURLOPT_URL, url.c_str());
    curl_easy_setopt(*ppCurlHandle, CURLOPT_HTTPGET, 1L);
    //curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    
    curl_easy_setopt(*ppCurlHandle, CURLOPT_WRITEFUNCTION, doCurlWriteCB);/* send all data to this function  */
    /* we pass our 'chunk' struct to the callback function */
    HttpReply httpReply;
    curl_easy_setopt(*ppCurlHandle, CURLOPT_WRITEDATA, (void *)&httpReply);
    curl_easy_setopt(*ppCurlHandle, CURLOPT_FOLLOWLOCATION, 1L); // tell us to follow redirection if redirected is need
    res = curl_easy_perform(*ppCurlHandle);
    if(res != CURLE_OK) {
        closeConn();// go around broken connection.
        SPDLOG_ERROR("curl_easy_perform() failed:{0}, URL:{1}", curl_easy_strerror(res), url);
        return EHTTP_GET_FAILURE;
    }  

    reply.assign(httpReply.response, httpReply.size);

    if (httpReply.response) {
        delete httpReply.response;
        httpReply.size = 0;
    }

    return EHTTP_OK;
    
}

int HttpClient::Put(const std::string& url, HttpReply& reply) {
    std::lock_guard<std::mutex> lk(execlusive_op_protect);
}

int HttpClient::Post(const std::string& url, HttpReply& reply) {
    std::lock_guard<std::mutex> lk(execlusive_op_protect);
}

bool HttpClient::connIsOpened() {
    return connOpened;
}

int HttpClient::openConn() {
    *ppCurlHandle = curl_easy_init();
    if(!(*ppCurlHandle)) {
        SPDLOG_ERROR("curl_easy_init() failed, the connection to remote is not establised");
        return EHTTP_OPEN_CONN_FAILURE;
    }

    connOpened = true;
    return EHTTP_OK;
}

void HttpClient::closeConn() {
    if (*ppCurlHandle) {
        curl_easy_cleanup(*ppCurlHandle);
    }
    
    *ppCurlHandle = nullptr;
    connOpened = false;
}