#ifndef LICENSE_CLIENT_HH

#define LICENSE_CLIENT_HH

#include <iostream>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <string>
#include <map>
#include <grpcpp/grpcpp.h>
#include <unistd.h>
#include "license.grpc.pb.h"
#include "lics_error.h"

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

/*
    NOTICES:
    algorithm id is very important, id depend by vendor.
*/
// start to add
#define UNIS_OD  (100)
#define UNIS_FACE_OA (101)
#define UNIS_VAS_OA (102)
// end to add

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using UnisAlgoLics::CreateLicsRequest;
using UnisAlgoLics::CreateLicsResponse;
using UnisAlgoLics::DeleteLicsRequest;
using UnisAlgoLics::DeleteLicsResponse;
using UnisAlgoLics::QueryLicsRequest;
using UnisAlgoLics::QueryLicsResponse;
using UnisAlgoLics::GetAuthAccessRequest;
using UnisAlgoLics::GetAuthAccessResponse;
using UnisAlgoLics::KeepAliveRequest;
using UnisAlgoLics::KeepAliveResponse;
using UnisAlgoLics::Algorithm;
using UnisAlgoLics::License;
using UnisAlgoLics::TaskType;
using UnisAlgoLics::AlgoLics;
using UnisAlgoLics::Vendor;

typedef struct LicsEvent_s {
    int id;
}LicsEvent;

class LicsClient {
public:
    LicsClient(std::shared_ptr<Channel> channel);
    ~LicsClient();

    int CreateLics(CreateLicsRequest& req, CreateLicsResponse& resp);
    int DeleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp);
    void QueryLics();

private:
    int getAuthAccess();
    int keepAlive();
    void doLoop();
    std::shared_ptr<LicsEvent> dequeue(); // TODO: make LicsEvent to be  a template
    void enqueue(std::shared_ptr<LicsEvent> t);
    bool empty();

    long getToken();
    void setToken(long token);

private:
    long token_{-1};

    std::map<long, std::shared_ptr<AlgoLics>> cache_;

    std::unique_ptr<License::Stub> stub_;

    // to be fixed: keep synchronized 
    std::atomic<bool> connected_ {false}; // license server receving client request.
    std::atomic<bool> running_{true};

    std::list<std::shared_ptr<LicsEvent>> event_;

    /*
    this mutex is used for the following purposes:
    1. keep synchronization primitive access to queue, like enqueue or dequeue;
    2. for the condition variable :cv_of_event_
    */
    std::mutex mtx_of_event_;
    std::condition_variable cv_of_event_;
};

#endif