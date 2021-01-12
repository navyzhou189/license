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
#include "lics_interface.h"

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"


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

enum LicsClientEventType {
    EXIT = 0,
};

class LicsClientEvent {
public:
    LicsClientEvent(LicsClientEventType type, int id = -1) : type_(type), id_(id) {}

    LicsClientEventType GetEventType() { return type_; }

private:
    LicsClientEventType type_;
    int id_;
};

class LicsClient {
public:
    LicsClient(std::shared_ptr<Channel> channel, AlgoCapability* algoLics, int size);
    ~LicsClient();

    int CreateLics(CreateLicsRequest& req, CreateLicsResponse& resp);
    int DeleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp);
    int GetAuthAccess();
    int KeepAlive();
    //void QueryLics();
    int GetTaskTypeFromAlgoID(int algoID, TaskType& type);

    void Stop();

protected:
    virtual Status createLics(CreateLicsRequest& req, CreateLicsResponse& resp);
    virtual Status deleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp);
    virtual Status getAuthAccess(const GetAuthAccessRequest& req, GetAuthAccessResponse& resp);
    virtual Status keepAlive(const KeepAliveRequest& req, KeepAliveResponse& resp);
    //void queryLics_();

private:
    void doLoop();
    std::shared_ptr<LicsClientEvent> dequeue(); // TODO: make LicsClientEvent to be  a template
    void enqueue(std::shared_ptr<LicsClientEvent> t);
    bool empty();

    void signalExit();// signal work thread to exit.
    bool gotExitSignal(std::shared_ptr<LicsClientEvent> t);

    long getToken();
    void setToken(long token);

private:
    long token_{-1};

    std::map<long, std::shared_ptr<AlgoLics>> cache_; // key is algorithm id

    std::unique_ptr<License::Stub> stub_;

    // to be fixed: keep synchronized 
    std::atomic<bool> connected_ {false}; // license server receving client request.
    std::atomic<bool> running_{true};

    std::list<std::shared_ptr<LicsClientEvent>> event_;

    /*
    this mutex is used for the following purposes:
    1. keep synchronization primitive access to queue, like enqueue or dequeue;
    2. for the condition variable :cv_of_event_
    */
    std::mutex exclusive_write_or_read_event;
    std::condition_variable cv_of_event_;
};

#endif