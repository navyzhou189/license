#include <iostream>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "license.grpc.pb.h"
#include "lics_error.h"

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
using UnisAlgoLics::License;


typedef struct LicsEvent_s {
    int id;
}LicsEvent;

class LicsClient {
public:
    LicsClient(std::shared_ptr<Channel> channel);

    int CreateLics();
    int DeleteLics();
    void QueryLics();

private:
    bool getAuthAccess();
    int keepAlive();
    void doLoop();
    std::shared_ptr<LicsEvent> dequeue(); // TODO: make LicsEvent to be  a template
    void enqueue(std::shared_ptr<LicsEvent> t);
    bool empty();

private:
    std::unique_ptr<License::Stub> stub_;

    // to be fixed: keep synchronized 
    bool connected_ {false}; // license server receving client request.
    bool running_{true};

    std::list<std::shared_ptr<LicsEvent>> event_;

    /*
    this mutex is used for the following purposes:
    1. keep synchronization primitive access to queue, like enqueue or dequeue;
    2. for the condition variable :cv_of_event_
    */
    std::mutex mtx_of_event_;
    std::condition_variable cv_of_event_;
};

LicsClient::LicsClient(std::shared_ptr<Channel> channel)
    : stub_(License::NewStub(channel)) {
        // TODO: start a doLoop thread


    }

int LicsClient::CreateLics(){
    if (!connected_) {
        std::cout << "disconnected to license server, connecting to license server..." << std::endl;
        if (!getAuthAccess()) {
            std::cout << "failed to connect to license server: CreateLics" << std::endl;
            return ELICS_NOK;
        }
        std::cout << "connected to license server: ok" << std::endl;
    }

    // Data we are sending to the server.
    CreateLicsRequest req;

    // Container for the data we expect from the server.
    CreateLicsResponse resp;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    Status status = stub_->CreateLics(&context, req, &resp);

    // Act upon its status.
    if (status.ok()) {
        return ELICS_OK;
    } else {
        std::cout << status.error_code() << ": " << status.error_message()
                    << std::endl;
        return ELICS_NOK;
    }
}

int LicsClient::DeleteLics() {
    if (!connected_) {
        std::cout << "disconnected to license server, connecting to license server..." << std::endl;
        if (!getAuthAccess()) {
            std::cout << "failed to connect to license server: DeleteLics" << std::endl;
            return ELICS_NOK;
        }
        std::cout << "connected to license server: ok" << std::endl;
    }

    DeleteLicsRequest req;
    DeleteLicsResponse resp;
    ClientContext context;
    Status status = stub_->DeleteLics(&context, req, &resp);
    if (status.ok()) {
        return ELICS_OK;
    } else {
        std::cout << status.error_code() << ": " << status.error_message()
                    << std::endl;
        return ELICS_NOK;
    }

}

void LicsClient::QueryLics() {
    if (!connected_) {
        std::cout << "disconnected to license server, connecting to license server..." << std::endl;
        if (!getAuthAccess()) {
            std::cout << "failed to connect to license server: QueryLics" << std::endl;
            return;
        }
        std::cout << "connected to license server: ok" << std::endl;
    }

    QueryLicsRequest req;
    QueryLicsResponse resp;
    ClientContext context;
    Status status = stub_->QueryLics(&context, req, &resp);
}

bool LicsClient::getAuthAccess() {
    GetAuthAccessRequest req;
    GetAuthAccessResponse resp;
    ClientContext context;
    Status status = stub_->GetAuthAccess(&context, req, &resp);
    if (status.ok()) {
        return true;
    }

    return false;
}


int LicsClient::keepAlive() {
    KeepAliveRequest req;
    KeepAliveResponse resp;
    ClientContext context;
    Status status = stub_->KeepAlive(&context, req, &resp);

    // TODO: parse the response, 
    if (status.ok()) {
        return ELICS_OK;
    }

    return ELICS_NOK;
}

bool LicsClient::empty() {
    if (event_.size() == 0) {
        return true;
    }

    return false;
}

std::shared_ptr<LicsEvent> LicsClient::dequeue() {
    std::unique_lock<std::mutex> lk(mtx_of_event_);
    if (cv_of_event_.wait_for(lk,std::chrono::milliseconds(100), [&]{return !empty();})) {

        std::shared_ptr<LicsEvent> job = std::move(event_.front());
        event_.pop_front();
        return job;
    } else {
        return nullptr;
    }
}

void LicsClient::enqueue(std::shared_ptr<LicsEvent> t) {
    {
        // bug to be fixed: mutex will block the client, we will fixed during some time in future.
        std::lock_guard<std::mutex> lk(mtx_of_event_);

        // TODO: push into queue
        event_.push_back(t);
    }
    

    cv_of_event_.notify_one(); // does not need lock to hold for notification.

}

void LicsClient::doLoop() {

    while (running_) {

    // query local license cache

    // send authentication request to license server
    if (!getAuthAccess()) {
        break;
    }

    connected_ = true;// bug to be fixed: keep it synchrozed

    // if ok, start keepAlive execution
    while(true) {
        // check if event happens
        

        // TODO: send keepavlie request to license server with license cache.
        int ret = keepAlive();
        if (ret != ELICS_OK) {
            std::cout << "keepAlive has a error:" << ret << std::endl;
            connected_ = false; // bug to be fixed
            break;
        }

        // TODO: update license cache about picture
        
        std::shared_ptr<LicsEvent> ev = dequeue();
        if (ev) {
            // TODO : send delete license request 

            // TODO: if failed , push the event to queue.
            // else update license cache about video
        }
        

        // TODO: sleep strategy

    }


    // TODO: sleep strategy
    }
}

int main(int argc, char** argv)
{
    std::string remote("localhost:50057"); // TODO: load from conf file.
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));
    int resp = client.CreateLics();
    std::cout << "client CreateLics " << resp << std::endl;

    return 0;
}