#ifndef LICENSE_HTTP_HH
#define LICENSE_HTTP_HH

#include <string>
#include <mutex>
#include <curl/curl.h>

#define EHTTP_OK    (0)
#define EHTTP_OPEN_CONN_FAILURE   (100)
#define EHTTP_GET_FAILURE   (101)


struct HttpReply {
   char *response{nullptr};
   size_t size{0};
};

// one HttpClient run in one single thread
// Post/Put/Get is a short-connection-based method, which will do tcp handshake, data transfer and tcp termination whenever calling method.
// HttpClient will allocate HttpReply.response, but app layor need to hold the responsability of memory free, like delete HttpReply.response.
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    int Post(const std::string& url, HttpReply& reply);
    int Put(const std::string& url, HttpReply& reply);
    int Get(const std::string& url, std::string& reply);
private:
    bool connIsOpened();
    int openConn();
    void closeConn();
private:
    bool connOpened{false};
    CURL** ppCurlHandle{nullptr};
    std::mutex execlusive_op_protect;
};



#endif