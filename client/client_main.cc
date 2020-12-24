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
using UnisAlgoLics::Algorithm;
using UnisAlgoLics::License;
using UnisAlgoLics::TaskType;
using UnisAlgoLics::AlgoLics;
using UnisAlgoLics::Vendor;

/*
    NOTICES:
    algorithm id is very important, id depend by vendor.
*/
// start to add
#define UNIS_OD  (100)
#define UNIS_FACE_OA (101)
#define UNIS_VAS_OA (102)
// end to add

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
    bool getAuthAccess();
    int keepAlive();
    void doLoop();
    std::shared_ptr<LicsEvent> dequeue(); // TODO: make LicsEvent to be  a template
    void enqueue(std::shared_ptr<LicsEvent> t);
    bool empty();

    long getToken();

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

LicsClient::~LicsClient() {
}

LicsClient::LicsClient(std::shared_ptr<Channel> channel)
    : stub_(License::NewStub(channel)) {
    // TODO: start a doLoop thread


    // load all license into cache, depend by vendor
    std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
    odLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    odLics->mutable_algo()->set_type(TaskType::VIDEO);
    odLics->mutable_algo()->set_algorithmid(UNIS_OD);
    odLics->set_requestid(-1);
    odLics->set_totallics(0);
    odLics->set_usedlics(0);
    cache_[UNIS_OD] = odLics;

    std::shared_ptr<AlgoLics> faceOaLics = std::make_shared<AlgoLics>();
    faceOaLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    faceOaLics->mutable_algo()->set_type(TaskType::PICTURE);
    faceOaLics->mutable_algo()->set_algorithmid(UNIS_FACE_OA);    
    faceOaLics->set_requestid(-1);
    faceOaLics->set_totallics(0);
    faceOaLics->set_usedlics(0);
    cache_[UNIS_FACE_OA] = faceOaLics;

    std::shared_ptr<AlgoLics> vasOaLics = std::make_shared<AlgoLics>();
    vasOaLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    vasOaLics->mutable_algo()->set_type(TaskType::PICTURE);
    vasOaLics->mutable_algo()->set_algorithmid(UNIS_VAS_OA);
    vasOaLics->set_requestid(-1);
    vasOaLics->set_totallics(0);
    vasOaLics->set_usedlics(0);
    cache_[UNIS_VAS_OA] = vasOaLics;


}

int LicsClient::CreateLics(CreateLicsRequest& req, CreateLicsResponse& resp){
    if (!connected_) {
        std::cout << "disconnected to license server, connecting to license server..." << std::endl;
        if (!getAuthAccess()) {
            std::cout << "failed to connect to license server: CreateLics" << std::endl;
            return ELICS_NOK;
        }
        std::cout << "connected to license server: ok" << std::endl;
    }

   if (req.algo().type() == TaskType::VIDEO) {

        req.set_token(getToken());

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

    if (req.algo().type() == TaskType::PICTURE) {
        // TODO: add lock
        auto search = cache_.find(req.algo().algorithmid());
        if (search != cache_.end()) {
            int total = search->second->totallics();
            int used = search->second->usedlics();
            int expectd =  req.clientexpectedlicsnum();
            int actual = (total - used) > expectd ? expectd : total - used;
            resp.set_clienggetactuallicsnum(actual);

            search->second->set_usedlics(used + actual);
        }
    }

    return ELICS_NOK;   
}

int LicsClient::DeleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp) {
    if (!connected_) {
        std::cout << "disconnected to license server, connecting to license server..." << std::endl;
        if (!getAuthAccess()) {
            std::cout << "failed to connect to license server: DeleteLics" << std::endl;
            return ELICS_NOK;
        }
        std::cout << "connected to license server: ok" << std::endl;
    }

    
   if (req.algo().type() == TaskType::VIDEO) {
        req.set_token(getToken());
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

    if (req.algo().type() == TaskType::PICTURE) {
        // TODO: add lock
        auto search = cache_.find(req.algo().algorithmid());
        if (search != cache_.end()) {
            int used = req.licsnum() + search->second->usedlics();
            resp.set_licsnum(used);

            search->second->set_usedlics(used);
        }
    }

    return ELICS_NOK; 
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

long LicsClient::getToken() {
    return token_;
}

bool LicsClient::getAuthAccess() {
    GetAuthAccessRequest req;
    req.set_token(getToken());
    GetAuthAccessResponse resp;
    ClientContext context;
    Status status = stub_->GetAuthAccess(&context, req, &resp);
    if (status.ok()) {
        std::cout << " get accessed token:" << resp.token() << "  ok" << std::endl;
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

        std::shared_ptr<LicsEvent> ev = std::move(event_.front());
        event_.pop_front();
        return ev;
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

        // send authentication request to license server
        if (!getAuthAccess()) {
            // TODO: sleep strategy
            continue;
        }

        connected_ = true;// bug to be fixed: keep it synchronized

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
    }
}

int main(int argc, char** argv)
{
    std::string remote("localhost:50057"); // TODO: load from conf file.
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest req;
    req.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    req.mutable_algo()->set_type(TaskType::VIDEO);
    req.mutable_algo()->set_algorithmid(UNIS_OD);

    // Container for the data we expect from the server.
    CreateLicsResponse resp;

    int ret = client.CreateLics(req, resp);
    std::cout << "client CreateLics: " << ret << std::endl;

    return 0;
}