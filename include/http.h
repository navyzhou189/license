#ifndef LICENSE_HTTP_HH
#define LICENSE_HTTP_HH

#include <string>
#include <mutex>
#include <curl/curl.h>

#define EHTTP_OK    (0)
#define EHTTP_OPEN_CONN_FAILURE   (100)


class HttpClientHandle {
public:
    HttpClientHandle() { curl = nullptr;}
    CURL* curl{nullptr};
};

// one HttpClient run on one single thread
// there is only one interface running at the same time: Post/Put/Get
class HttpClient {
public:
    HttpClient();
    ~HttpClient();
    int Post(const std::string& req, const std::string& resp);
    int Put(const std::string& req, const std::string& resp);
    int Get(const std::string& req, const std::string& resp);
private:
    bool connIsOpened();
    int openConn();
    void closeConn();
private:
    bool connOpened{false};
    HttpClientHandle handle;
    std::mutex mtx;
};



#endif