#include "client.h"

LicsClient::~LicsClient() {
    running_= false;
    sleep(1); // sleep for a delay to exit doLoop thread.
}


std::atomic<bool> logStartup{false};
LicsClient::LicsClient(std::shared_ptr<Channel> channel)
    : stub_(License::NewStub(channel)) {
    
    if (!logStartup) {
        logStartup = true;
        auto log = spdlog::rotating_logger_mt("client", "/var/unis/license/client/log/log.txt", 1048576 * 5, 3);
        log->flush_on(spdlog::level::info); //set flush policy 
        spdlog::set_default_logger(log); // set log to be defalut 
        spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l [%s:%!:%#] %v"); 
    }


    // TODO: start a doLoop thread
    std::thread t(&LicsClient::doLoop, this);
    t.detach();


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
        SPDLOG_INFO("disconnected to license server, connecting to license server...");
        int ret = getAuthAccess();
        if (ELICS_OK != ret) {
            SPDLOG_INFO("failed to connect to license server: CreateLics({0})", ret);
            return ret;
        }
        SPDLOG_INFO("connected to license server: ok");
    }

   if (req.algo().type() == TaskType::VIDEO) {

        req.set_token(getToken());

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = stub_->CreateLics(&context, req, &resp);

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
        }
    }

    return ELICS_UNKOWN_ERROR;   
}

int LicsClient::DeleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp) {
    if (!connected_) {
        SPDLOG_INFO("disconnected to license server, connecting to license server...");
        int ret = getAuthAccess();
        if (ELICS_OK != ret) {
            SPDLOG_INFO("failed to connect to license server: DeleteLics({0})", ret);
            return ret;
        }
        SPDLOG_INFO("connected to license server: ok");
    }

    
   if (req.algo().type() == TaskType::VIDEO) {
        req.set_token(getToken());
        ClientContext context;
        Status status = stub_->DeleteLics(&context, req, &resp);
        if (status.ok()) {
            return ELICS_OK;
        } else {
            SPDLOG_INFO("DeleteLics({0}):{1}", status.error_code(), status.error_message());
            return status.error_code();
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

    return ELICS_UNKOWN_ERROR; 
}

void LicsClient::QueryLics() {
    if (!connected_) {
        SPDLOG_INFO("disconnected to license server, connecting to license server...");
        int ret = getAuthAccess();
        if (ELICS_OK != ret) {
            SPDLOG_INFO("failed to connect to license server: QueryLics({0})", ret);
            return; // TODO: throw error
        }
        SPDLOG_INFO("connected to license server: ok");
    }

    QueryLicsRequest req;
    QueryLicsResponse resp;
    ClientContext context;
    Status status = stub_->QueryLics(&context, req, &resp);
}

long LicsClient::getToken() {
    return token_;
}

int LicsClient::getAuthAccess() {
    GetAuthAccessRequest req;
    req.set_token(getToken());
    GetAuthAccessResponse resp;
    ClientContext context;
    Status status = stub_->GetAuthAccess(&context, req, &resp);
    if (status.ok()) {
        SPDLOG_INFO(" get accessed token: {0} ok", resp.token());
        return resp.respcode();
    }

    return status.error_code();
}


int LicsClient::keepAlive() {
    KeepAliveRequest req;
    KeepAliveResponse resp;
    ClientContext context;

    // TODO: set client maximum limit
    //req.set_maxlimit(100);
    Status status = stub_->KeepAlive(&context, req, &resp);

    // TODO: parse the response, 
    if (status.ok()) {
        return ELICS_OK;
    }

    return ELICS_UNKOWN_ERROR;
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
        if (ELICS_OK != getAuthAccess()) { // bug to be fixed: make getAuthAcess being automical operation
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
                SPDLOG_ERROR("keepAlive has a error: {0}", ret);
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
            sleep(1);
        }
    }
}

