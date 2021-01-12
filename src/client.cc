#include "client.h"

std::atomic<bool> clientStartup_{false};
std::shared_ptr<LicsClient> licsClient_ = nullptr;

const char*lics_version() {
    return "UNIS_LICS_CLIENT_V1.0.0";
}

int init_resource_before_client_startup() {
    if (clientStartup_) {
        return ELICS_DUPLICATED_RESOURCE_INIT;
    }

    clientStartup_ = true;
    return ELICS_OK;
}

void cleanup_resource_before_client_exit() {
    
}


/*
* why design this fuction?
* only used make a unit-test for lics_global_init, do not call it directoly.
*/
int lics_global_init_internal(const char* remote, AlgoCapability* algoLics, int size, std::shared_ptr<LicsClient> client) {

    int ret = init_resource_before_client_startup();
    if (ret != ELICS_OK) {
        return ret;
    }
    
    licsClient_ = client;

    sleep(1);// make some time to get LicsClient ready to startup.
    return ELICS_OK;
}

int lics_global_init(const char* remote, AlgoCapability* algoLics, int size) {

    int ret = init_resource_before_client_startup();
    if (ret != ELICS_OK) {
        return ret;
    }
    
    licsClient_ = std::make_shared<LicsClient>(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()), algoLics, size);

    sleep(1);// make some time to get LicsClient ready to startup.
    return ELICS_OK;
}

int lics_apply(int algoID, const int expectLicsNum, int* actualLicsNum) {
    if (!clientStartup_) {
        return ELICS_UNITILIZED_RESOURCE;
    }

    TaskType type;
    int ret = licsClient_->GetTaskTypeFromAlgoID(algoID, type);
    if (ret != ELICS_OK) {
        return ret;
    }

    CreateLicsRequest createReq;
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(type);
    createReq.mutable_algo()->set_algorithmid(algoID);
    createReq.set_clientexpectedlicsnum(expectLicsNum);

    CreateLicsResponse createResp;
    ret = licsClient_->CreateLics(createReq, createResp);
    if (ret != ELICS_OK) {
        return ret;
    }

    *actualLicsNum = createResp.clientgetactuallicsnum();

    return ret;
}

int lics_free(int algoID, const int licsNum) {
    if (!clientStartup_) {
        return ELICS_UNITILIZED_RESOURCE;
    }

    TaskType type;
    int ret = licsClient_->GetTaskTypeFromAlgoID(algoID, type);
    if (ret != ELICS_OK) {
        return ret;
    }

    DeleteLicsRequest deleteReq;
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(type);
    deleteReq.mutable_algo()->set_algorithmid(algoID);
    deleteReq.set_licsnum(licsNum);

    DeleteLicsResponse deleteResp;

    return licsClient_->DeleteLics(deleteReq, deleteResp);
}

void lics_global_cleanup() {
    if (!clientStartup_) {
        return;
    }

    licsClient_->Stop();

    sleep(1); // make some time to signal thread to exit.

    licsClient_.reset();

    cleanup_resource_before_client_exit();

    clientStartup_ = false; 
}

LicsClient::~LicsClient() {
    running_= false;
    spdlog::shutdown();// exit log
}

int LicsClient::GetTaskTypeFromAlgoID(int algoID, TaskType& type) {
    //TODO: add lock
    auto search = cache_.find(algoID);
    if (search != cache_.end()) {
        type = search->second->algo().type();
        return ELICS_OK;
    }
    return ELICS_INVALID_PARAMS;
}

LicsClient::LicsClient(std::shared_ptr<Channel> channel, AlgoCapability* algoLics, int size)
    : stub_(License::NewStub(channel)) {

    // load log 
    auto log = spdlog::rotating_logger_mt("client", "/var/unis/license/client/log/log.txt", 1048576 * 5, 3);
    log->flush_on(spdlog::level::info); //set flush policy 
    spdlog::set_default_logger(log); // set log to be defalut 
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l [%s:%!:%#] %v"); 
    
    for (int idx = 0; idx < size; ++idx) {
        std::shared_ptr<AlgoLics> lics = std::make_shared<AlgoLics>();
        lics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
        /*
        * why do we need a duplicated definition about type from lics_interface.h,
        * because we have client a full isolation from  algorithm lics, it's up to
        * app, but the trade-off is that we need to maintain the same content between
        * AlgoLicsType and TaskType.
        */
        lics->mutable_algo()->set_type((TaskType)algoLics[idx].type);
        lics->mutable_algo()->set_algorithmid(algoLics[idx].algoID);
        lics->set_requestid(-1);
        lics->set_totallics(0);
        lics->set_usedlics(0);
        lics->set_maxlimit(algoLics[idx].maxLimit); // TODO: set by app
        cache_[algoLics[idx].algoID] = lics;
    }

    // TODO: start a doLoop thread
    std::thread t(&LicsClient::doLoop, this);
    t.detach();
}

Status LicsClient::createLics(CreateLicsRequest& req, CreateLicsResponse& resp){
    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // The actual RPC.
    return stub_->CreateLics(&context, req, &resp);
}

int LicsClient::CreateLics(CreateLicsRequest& req, CreateLicsResponse& resp){
    if (!connected_) {
        SPDLOG_INFO("disconnected to license server, please wait and retry...");
        // TODO: trigger an event to keepalive thread and reconnect to license server.
        return ELICS_NET_DISCONNECTED;
    }

   if (req.algo().type() == TaskType::VIDEO) {

        req.set_token(getToken());

        Status status = createLics(req, resp);

        // Act upon its status. 
        // TODO: function is timeout or other error.
        if (status.ok()) {
            return ELICS_OK;
        } else {
            SPDLOG_INFO("CreateLics({0}):{1}", status.error_code(), status.error_message());
            return status.error_code();
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
            resp.set_clientgetactuallicsnum(actual);

            search->second->set_usedlics(used + actual);
            return ELICS_OK;
        } else {
            SPDLOG_WARN("algorithm({0}) id not exist", req.algo().algorithmid());
            return ELICS_ALGO_NOT_EXIST;
        }
    }

    SPDLOG_ERROR("unsupport task type:{0}", req.algo().type());
    return ELICS_UNKOWN_ERROR;   
}


Status LicsClient::deleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp) {
    ClientContext context;
    return stub_->DeleteLics(&context, req, &resp);
}

int LicsClient::DeleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp) {

    if (req.algo().type() == TaskType::VIDEO) {
        if (!connected_) {
            SPDLOG_INFO("disconnected to license server, please wait and retry...");
            // TODO: trigger an event to keepalive thread and reconnect to license server.
            return ELICS_NET_DISCONNECTED;
        }

        req.set_token(getToken());
        
        Status status = deleteLics(req, resp);

        if (status.ok()) {
            return ELICS_OK;
        } else {
            SPDLOG_INFO("DeleteLics({0}):{1}", status.error_code(), status.error_message());
            return status.error_code();// app need to handle this error
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

        return ELICS_OK;
    }

    return ELICS_ALGO_NOT_EXIST; 
}

// int LicsClient::QueryLics() {
//     if (!connected_) {
//         SPDLOG_INFO("disconnected to license server, please wait and retry...");
//         // TODO: trigger an event to keepalive thread and reconnect to license server.
//         return ELICS_NET_DISCONNECTED;
//     }

//     QueryLicsRequest req;
//     QueryLicsResponse resp;
//     ClientContext context;
//     Status status = stub_->QueryLics(&context, req, &resp);
// }

long LicsClient::getToken() {
    return token_;
}

void LicsClient::setToken(long token) {
    token_ = token;
}

Status LicsClient::getAuthAccess(const GetAuthAccessRequest& req, GetAuthAccessResponse& resp) {
    ClientContext context;
    return stub_->GetAuthAccess(&context, req, &resp);
}

int LicsClient::GetAuthAccess() {
    GetAuthAccessRequest req;
    req.set_token(getToken());
    GetAuthAccessResponse resp;
    Status status = getAuthAccess(req, resp);
    if (status.ok()) {
        SPDLOG_INFO(" get accessed token: {0} ok", resp.token());
        setToken(resp.token());
        return resp.respcode();
    }

    return status.error_code();
}

Status LicsClient::keepAlive(const KeepAliveRequest& req, KeepAliveResponse& resp) {
    ClientContext context;
    return stub_->KeepAlive(&context, req, &resp);
}

int LicsClient::KeepAlive() {
    KeepAliveRequest req;
    KeepAliveResponse resp;
    
    req.set_token(getToken());
    // upload picture lics to server
    for(auto iter : cache_) { 
        AlgoLics* lics = req.add_lics();
        lics->set_requestid(iter.second->requestid());
        lics->set_totallics(iter.second->totallics());
        lics->set_usedlics(iter.second->usedlics());
        lics->set_maxlimit(iter.second->maxlimit());

        lics->mutable_algo()->set_vendor(iter.second->algo().vendor());
        lics->mutable_algo()->set_type(iter.second->algo().type());
        lics->mutable_algo()->set_algorithmid(iter.second->algo().algorithmid());
    }

    Status status = keepAlive(req, resp);

    // TODO: parse the response, 
    if (status.ok()) {

        for (int idx = 0; idx < resp.lics_size(); ++idx ) {
            int algoID = resp.lics(idx).algo().algorithmid();
            int total = resp.lics(idx).totallics();

            auto search = cache_.find(algoID);
            if (search != cache_.end()) {
                cache_[algoID]->set_totallics(total);
            }
        }
        
        return ELICS_OK;
    }

    return status.error_code();
}

bool LicsClient::empty() {
    if (event_.size() == 0) {
        return true;
    }

    return false;
}

void LicsClient::Stop() {
    signalExit();
}

void LicsClient::signalExit() {
    std::shared_ptr<LicsClientEvent> t = std::make_shared<LicsClientEvent>(LicsClientEventType::EXIT);
    enqueue(t);
}

std::shared_ptr<LicsClientEvent> LicsClient::dequeue() {
    std::unique_lock<std::mutex> lk(mtx_of_event_);
    if (cv_of_event_.wait_for(lk,std::chrono::milliseconds(100), [&]{return !empty();})) {

        std::shared_ptr<LicsClientEvent> ev = std::move(event_.front());
        event_.pop_front();
        return ev;
    } else {
        return nullptr;
    }
}

void LicsClient::enqueue(std::shared_ptr<LicsClientEvent> t) {
    {
        // bug to be fixed: mutex will block the client, we will fixed during some time in future.
        std::lock_guard<std::mutex> lk(mtx_of_event_);

        // TODO: push into queue
        event_.push_back(t);
    }
    
    cv_of_event_.notify_one(); // does not need lock to hold for notification.
}

bool LicsClient::gotExitSignal(std::shared_ptr<LicsClientEvent> t) {

    if (t) {
        if (t->GetEventType() == LicsClientEventType::EXIT) {
            return true;
        }
    }

    return false;
    
}

void LicsClient::doLoop() {

    while (running_) {

        // send authentication request to license server
        if (ELICS_OK != GetAuthAccess()) { // bug to be fixed: make getAuthAcess being automical operation
            std::shared_ptr<LicsClientEvent> ev = dequeue();
            if (ev) {
                // TODO : send delete license request 
                if (gotExitSignal(ev)) {
                    return;
                }

                // TODO: if failed , push the event to queue.
                // else update license cache about video
            }
            continue;
        }

        connected_ = true;// bug to be fixed: keep it synchronized

        // if ok, start keepAlive execution
        while(true) {
            // check if a event comes.
            std::shared_ptr<LicsClientEvent> ev = dequeue();
            if (ev) {
                // TODO : send delete license request 
                if (gotExitSignal(ev)) {
                    return;
                }

                // TODO: if failed , push the event to queue.
                // else update license cache about video
            }

            // TODO: send keepavlie request to license server with license cache.
            int ret = KeepAlive();
            if (ret != ELICS_OK) {
                SPDLOG_ERROR("keepAlive has a error: {0}", ret);
                connected_ = false; // bug to be fixed
                break;
            }

            // TODO: update license cache about picture
            

            
        }
    }
}

