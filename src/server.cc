#include "server.h"
#include <ctime>

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"


#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG // set level beyond debug when put it into production 
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // must be included if log user defined object
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

#define SERVER_CONF_FILE   ("/var/unis/license/server/conf/server.conf")

class ServerConf {
public:
    ServerConf(const std::string& file) {
        std::string line;
        std::ifstream confFile (file);
        if (confFile.is_open())
        {
            while ( getline (confFile,line) )
            {
                parse(line);
            }
            confFile.close();
        } else {
            SPDLOG_ERROR("failed to open file:{0}", file);
            abort();
        }

        confFile.close();
    }

    std::string GetItem(const std::string& key) {
        auto search = conf_.find(key);
        if (search != conf_.end()) {
            return search->second;
        }

        return std::string("");
    }

private:
    // trim all space and newline(\r\n or \r) characters
    std::string trim(const std::string& str) {
        std::string trimStr;

        for (auto ch = str.begin(); ch != str.end(); ++ch) {
            if ((*ch == '\r') || (*ch == '\n') || (*ch == ' ') ) {
                continue;
            }

            trimStr.push_back(*ch);
        }

        return trimStr;
    }

    void parse(const std::string& line) {
        std::string key;
        std::string value;

        size_t pos = line.find_first_of("=");
        if (std::string::npos == pos) {
            return;
        }
        key = trim(line.substr(0, pos)); // substr return a [pos, pos + count) substring
        value = trim(line.substr(pos + 1, line.length()));

        conf_[key] = value;

        return;
    }

private:
    std::map<std::string, std::string> conf_;

};

static std::mutex mtxOfLics;
static std::shared_ptr<HttpClient> httpClientOfLics = nullptr;
static std::shared_ptr<ServerConf> srvConfOfLics = nullptr;

std::shared_ptr<HttpClient> getHttpClient() {
    std::lock_guard<std::mutex> lk(mtxOfLics);
    if (!httpClientOfLics) {
        httpClientOfLics = std::make_shared<HttpClient>();
    }

    return httpClientOfLics;
}

std::shared_ptr<ServerConf> getServerConf() {
    std::lock_guard<std::mutex> lk(mtxOfLics);
    if (!srvConfOfLics) {
        srvConfOfLics = std::make_shared<ServerConf>(SERVER_CONF_FILE);
    }

    return srvConfOfLics;
}

long GetTimeSecsFromEpoch() {
    std::time_t result = std::time(nullptr);
    return result;
}

Client::Client(long token, std::map<long, std::shared_ptr<AlgoLics>> a) : clientToken(token), algo(a) {
    timestamp = GetTimeSecsFromEpoch();
}

std::map<long, std::shared_ptr<AlgoLics>> Client::Algos() {
    return algo;
}

void Client::UpdateTimestamp() {
    // TODO: update timestamp with system;
    timestamp = GetTimeSecsFromEpoch();
    ZeroHeartbeatTimeoutCnt();
}

int Client::HeartbeatTimeoutCnt() {
    return continusKeepAliveFailedCnt;
}

void Client::IncHeartbeatTimeoutCnt() {
    ++continusKeepAliveFailedCnt;
}

void Client::ZeroHeartbeatTimeoutCnt() {
    continusKeepAliveFailedCnt = 0;
}

long Client::GetLatestTimestamp() {
    return timestamp;
}

long Client::GetToken() {
    return clientToken;
}

bool Client::HaveAlgoID(long algoID) {
    auto search = algo.find(algoID);
    if (search != algo.end()) {
        return true;
    }

    return false;
}

bool Client::Alive() {

    // TODO: read max keep alive from conf
    if (continusKeepAliveFailedCnt > 3) {
        return false;
    }

    return true;
}

void Client::AddLics(long algoID, int num) {
    auto search = algo.find(algoID);
    if (search == algo.end()) {
        return;
    }

    int used = search->second->usedlics();
    search->second->set_usedlics(used + num);
    search->second->set_totallics(used + num);
}

void Client::DecLics(long algoID, int num) {
    auto search = algo.find(algoID);
    if (search == algo.end()) {
        return;
    }

    int used = search->second->usedlics();
    search->second->set_usedlics(used - num);
    search->second->set_totallics(used - num);
}

LicsServer::LicsServer() {
    auto log = spdlog::rotating_logger_mt("server", getServerConf()->GetItem("log"), 1048576 * 5, 3);
    log->flush_on(spdlog::level::debug); //set flush policy 
    spdlog::set_default_logger(log); // set log to be defalut 
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l [%s:%!:%#] %v");   
    spdlog::set_level(spdlog::level::debug);

    // load all license into cache, depend by vendor
    std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
    odLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    odLics->mutable_algo()->set_type(TaskType::VIDEO);
    odLics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
    odLics->set_requestid(-1);
    odLics->set_totallics(0);
    odLics->set_usedlics(0);
    odLics->set_maxlimit(0); // TODO: set by conf
    licenseQ[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD] = odLics;

    std::shared_ptr<AlgoLics> faceOaLics = std::make_shared<AlgoLics>();
    faceOaLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    faceOaLics->mutable_algo()->set_type(TaskType::PICTURE);
    faceOaLics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA);    
    faceOaLics->set_requestid(-1);
    faceOaLics->set_totallics(0);
    faceOaLics->set_usedlics(0);
    faceOaLics->set_maxlimit(0);
    licenseQ[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA] = faceOaLics;

    std::shared_ptr<AlgoLics> vasOaLics = std::make_shared<AlgoLics>();
    vasOaLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    vasOaLics->mutable_algo()->set_type(TaskType::PICTURE);
    vasOaLics->mutable_algo()->set_algorithmid(UNIS_VAS_OA);
    vasOaLics->set_requestid(-1);
    vasOaLics->set_totallics(0);
    vasOaLics->set_usedlics(0);
    vasOaLics->set_maxlimit(0);
    licenseQ[UNIS_VAS_OA] = vasOaLics;

    std::thread t(&LicsServer::doLoop, this);
    t.detach();
}

LicsServer::~LicsServer() {
    running_ = false;
    sleep(1);
    SPDLOG_ERROR("got exit, bye");
    spdlog::shutdown();// exit log
}

#define FETCH_ALGOS_TOTAL_LICS_URL  {\
std::string("http://" + getServerConf()->GetItem("cloud") + "/api/vcloud/v2/license/authinfos")\
}

void LicsServer::fetchAlgosTotalLicFromCloud(std::map<long, std::shared_ptr<AlgoLics>>& remote){
    std::string reply;
    int ret = getHttpClient()->Get(FETCH_ALGOS_TOTAL_LICS_URL, reply);
    if ( ret != EHTTP_OK) {
        SPDLOG_ERROR("GET from cloud have a error:{0}", ret);
        return;
    }
        

    SPDLOG_DEBUG("GET from cloud:{0}", reply.c_str());

    rapidjson::Document document;

    // absense of key field, like data or res, means that it's a bad response, throw it and return
    document.Parse(reply.c_str());
    if (!document.HasMember("data")) {
        SPDLOG_WARN("json parse failed: no found field(data) GET from cloud:{0}", reply.c_str());
        return;
    }

    if (!document["data"].HasMember("res")) {
        SPDLOG_WARN("json parse failed: no found field(res) GET from cloud:{0}", reply.c_str());
        return;
    }

    /*
    * using F as a function of total algorithm license number, input: agorithm id
    * F(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA) = F(VIASFACEP-MAX-CLASSES) + F(VIASCAR-MAX-CLASSES) / 24 / 3600 +
    *                                               F(VIASOA-MAX-CLASSES) + F(FVSAOA-MAX-CLASSES)
    * 
    * F(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD) = F(VIASVIDEO-MAX-CLASSES) + F(VIASFACEV-MAX-CLASSES) + F(VIASOD-MAX-CLASSES)
    *                                               + F(FVSAOD-MAX-CLASSES)
    */

    int numOfFacePersonVehicleNonOA = 0;
    int numOfFacePersonVehicleNonOD = 0;

    if (document["data"]["res"].HasMember("VIASFACEP-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASFACEP-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASFACEP-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOA += document["data"]["res"]["VIASFACEP-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("VIASCAR-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASCAR-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASCAR-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            int carLics = document["data"]["res"]["VIASCAR-MAX-CLASSES"]["num"].GetInt();
            int translatedLics = carLics / 24 / 3600;
            carLics = translatedLics <= 0 ? 0 : translatedLics;
            numOfFacePersonVehicleNonOA += translatedLics;
        }
    }

    if (document["data"]["res"].HasMember("VIASOA-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASOA-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASOA-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOA += document["data"]["res"]["VIASOA-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("FVSAOA-MAX-CLASSES")) {

        if (!document["data"]["res"]["FVSAOA-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under FVSAOA-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOA += document["data"]["res"]["FVSAOA-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("VIASVIDEO-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASVIDEO-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASVIDEO-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOD += document["data"]["res"]["VIASVIDEO-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("VIASFACEV-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASFACEV-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASFACEV-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOD += document["data"]["res"]["VIASFACEV-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("VIASOD-MAX-CLASSES")) {

        if (!document["data"]["res"]["VIASOD-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under VIASOD-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOD += document["data"]["res"]["VIASOD-MAX-CLASSES"]["num"].GetInt();
        }
    }

    if (document["data"]["res"].HasMember("FVSAOD-MAX-CLASSES")) {

        if (!document["data"]["res"]["FVSAOD-MAX-CLASSES"].HasMember("num")) {
            SPDLOG_WARN("json parse failed: no found field(num) under FVSAOD-MAX-CLASSES. GET from cloud:{0}", reply.c_str());
        }else {
            numOfFacePersonVehicleNonOD += document["data"]["res"]["FVSAOD-MAX-CLASSES"]["num"].GetInt();
        }
    }


    std::shared_ptr<AlgoLics> oaLics = std::make_shared<AlgoLics>();
    oaLics->set_totallics(numOfFacePersonVehicleNonOA);

    std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
    odLics->set_totallics(numOfFacePersonVehicleNonOD);
    

    remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA] = oaLics;
    remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD] = odLics;

}

void LicsServer::pushAlgosUsedLicToCloud(const std::map<long, std::shared_ptr<AlgoLics>>& local) {

}


void LicsServer::updateLocalLics(const std::map<long, std::shared_ptr<AlgoLics>>& remoteAlgosTotalLic) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);
    for (const auto &remote : remoteAlgosTotalLic) {
        licenseQ[remote.first]->set_totallics(remote.second->totallics());
    }
}

void LicsServer::getLocalLics(std::map<long, std::shared_ptr<AlgoLics>>& local) {

}

void LicsServer::Shutdown() {
    signalExit();
}

void LicsServer::signalExit() {
    std::shared_ptr<LicsServerEvent> t = std::make_shared<LicsServerEvent>(LicsServerEventType::EXIT);
    enqueue(t);
}

bool LicsServer::empty() {
    if (event_.size() == 0) {
        return true;
    }

    return false;
}

std::shared_ptr<LicsServerEvent> LicsServer::dequeue() {
    std::unique_lock<std::mutex> lk(exclusive_write_or_read_event);
    if (cv_of_event_.wait_for(lk,std::chrono::milliseconds(100), [&]{return !empty();})) {

        std::shared_ptr<LicsServerEvent> ev = std::move(event_.front());
        event_.pop_front();
        return ev;
    } else {
        return nullptr;
    }
}

void LicsServer::enqueue(std::shared_ptr<LicsServerEvent> t) {
    {
        // bug to be fixed: mutex will block the client, we will fixed during some time in future.
        std::lock_guard<std::mutex> lk(exclusive_write_or_read_event);

        // TODO: push into queue
        event_.push_back(t);
    }
    
    cv_of_event_.notify_one(); // does not need lock to hold for notification.
}

bool LicsServer::gotExitSignal(std::shared_ptr<LicsServerEvent> t) {

    if (t) {
        if (t->GetEventType() == LicsServerEventType::EXIT) {
            return true;
        }
    }

    return false;
    
}

void LicsServer::serverClearDeadClients() {

    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);

    long sysTime = GetTimeSecsFromEpoch();// prevent from mulitiple call in the loop
    for (auto clientIter = clientQ.begin(); clientIter != clientQ.end();) {
        // check if the client keep alive
        long token = clientIter->first;
        std::shared_ptr<Client> client(clientIter->second);
        long clientTime = client->GetLatestTimestamp();
        
        long diff = sysTime >= clientTime ? sysTime - clientTime : 0; 
        // TODO: get timeout from conf
        if (diff > 30) {
            client->IncHeartbeatTimeoutCnt();
            SPDLOG_INFO("client({0}) hearbeat timeout reach {1}", token, client->HeartbeatTimeoutCnt());
        }

        if (!client->Alive()) {
            std::map<long, std::shared_ptr<AlgoLics>> Lics = client->Algos();

            for (auto& lic : Lics) {
                long algoID = lic.first;
                int clientUsedLics = lic.second->totallics();
                auto algoInLicenseQ = licenseQ.find(algoID);
                if (algoInLicenseQ != licenseQ.end()) {
                    int usedLics = algoInLicenseQ->second->usedlics();
                    algoInLicenseQ->second->set_usedlics(usedLics - clientUsedLics);
                }
            }
            SPDLOG_INFO("detect heatbeat-stoped client. remove token:{0}", token);
            clientIter = clientQ.erase(clientIter);
        } else {
            ++clientIter;
        }
    }
}

void LicsServer::print() {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);

    std::string allLics("server licenses big picture\nalgo\t total\t used");
    for (auto& lics : licenseQ) {
        long algo = lics.first;
        int total = lics.second->totallics();
        int used = lics.second->usedlics();

        allLics += "\n" + std::to_string(algo) + "\t " + std::to_string(total) + "\t " + std::to_string(used);
    }
    SPDLOG_INFO(allLics);

    for (auto& c : clientQ) {
        long token = c.first;
        std::string clientLics("client({0}) license\nalgo\t total\t used");
        
        std::map<long, std::shared_ptr<AlgoLics>> algo = c.second->Algos();
        for (auto& a : algo) {
            long algo = a.first;
            int total = a.second->totallics();
            int used = a.second->usedlics();

            clientLics += "\n" + std::to_string(algo) + "\t " + std::to_string(total) + "\t " + std::to_string(used);
        }
        SPDLOG_INFO(clientLics, token);
    }
    
}

void LicsServer::doLoop() {
    while (running_) {

        std::shared_ptr<LicsServerEvent> ev = dequeue();
        if (ev) {
            // TODO : send delete license request 
            if (gotExitSignal(ev)) {
                SPDLOG_ERROR("got a signal to exit. bye");
                return;
            }

            // TODO: if failed , push the event to queue.
            // else update license cache about video
        }

        serverClearDeadClients();

        // TODO: call vcloud api to update license.
        std::map<long, std::shared_ptr<AlgoLics>> remoteAlgosTotalLic;
        fetchAlgosTotalLicFromCloud(remoteAlgosTotalLic);

        updateLocalLics(remoteAlgosTotalLic);

        std::map<long, std::shared_ptr<AlgoLics>> cacheAlgosUsedLic;
        getLocalLics(cacheAlgosUsedLic);

        pushAlgosUsedLicToCloud(cacheAlgosUsedLic);
        
        // TODO: get interval from conf
    
        print();
    }
}

void LicsServer::licsQuery(long token, long algoID, int& total, int& used) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);
    total = 0;
    used = 0;
    // add lock
    auto client = clientQ.find(token); // search client
    if (client == clientQ.end()) {
        SPDLOG_ERROR("client({0}) query license failed:no exist user", token);
        return ;
    }

    auto algo = licenseQ.find(algoID);
    if (algo == licenseQ.end()) {
        SPDLOG_ERROR("client({0}) query license failed:no exist algorithm id:{1}", token, algoID);
        return ;
    }

    // get total and used lics by algorithm id
    total = algo->second->totallics();
    used = algo->second->usedlics();
}

int LicsServer::totalClientNum() {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);
    return clientQ.size();
}

int LicsServer::clientNumByAlgoID(long algoID) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);
    int num = 0;
    for(auto client : clientQ) {
        if (client.second->HaveAlgoID(algoID)) {
            ++num;
        }
    }
    return num;
}

int LicsServer::licsAlloc(long token, long algoID, int expected) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);

    auto client = clientQ.find(token); // search client
    if (client == clientQ.end()) {
        SPDLOG_ERROR("client({0}) alloc license failed:no exist user", token);
        return 0;
    }

    auto algo = licenseQ.find(algoID);
    if (algo == licenseQ.end()) {
        SPDLOG_ERROR("client({0}) alloc license failed:no exist algorithm id:{1}", token, algoID);
        return 0;
    }

    if (algo->second->algo().type() != TaskType::VIDEO) {
        SPDLOG_ERROR("incorrect call, only support VIDEO lics alloc:client({0}), algorithm id({1})", token, algoID);
        return 0;
    }

    // get total and used lics by algorithm id
    int total = algo->second->totallics();
    int used = algo->second->usedlics();

    int actualAllocedLics = (total - used) >= expected ? expected : (total - used);
    algo->second->set_usedlics(used + actualAllocedLics);// update used licenses for algorithm
    client->second->AddLics(algoID, actualAllocedLics); // update used licenses for client

    return actualAllocedLics;
}

int LicsServer::licsFree(long token, long algoID, int expected) { 

    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);

    auto client = clientQ.find(token); // search client
    if (client == clientQ.end()) {
        SPDLOG_ERROR("client({0}) free license failed:no exist user", token);
        return 0;
    }

    auto algo = licenseQ.find(algoID);
    if (algo == licenseQ.end()) {
        SPDLOG_ERROR("client({0}) free license failed:no exist algorithm id:{1}", token, algoID);
        return 0;
    }

    int used = algo->second->usedlics();

    int actualFreeLics = used >= expected ? expected : used;
    algo->second->set_usedlics(used - actualFreeLics);// update used licenses for algorithm
    client->second->DecLics(algoID, actualFreeLics); // update used licenses for client

    return actualFreeLics;
}

long LicsServer::newClientToken() {

    // TODO: add lock
    tokenBase_++;
    return tokenBase_;
}


Status LicsServer::createLics(const CreateLicsRequest* request, CreateLicsResponse* response) {
    // when SPDLOG_ACTIVE_LEVEL macro beyond SPDLOG_LEVEL_DEBUG, all SPDLOG_DEBUG will be not compiled.
    SPDLOG_DEBUG("client({0}) send lics alloc request: vendor({1}), type({2}), algorithm_id({3}), expected_lics({4})", 
                request->token(),
                request->algo().vendor(),
                request->algo().type(),
                request->algo().algorithmid(),
                request->clientexpectedlicsnum());
    long clientToken = request->token();
    response->set_token(clientToken);
    response->set_requestid(0);// to be fixed
    response->mutable_algo()->set_vendor(request->algo().vendor());
    response->mutable_algo()->set_type(request->algo().type());
    response->mutable_algo()->set_algorithmid(request->algo().algorithmid());

    int licsNum = licsAlloc(clientToken, request->algo().algorithmid(), request->clientexpectedlicsnum());
    response->set_clientgetactuallicsnum(licsNum);
    response->set_respcode(ELICS_OK);

    SPDLOG_DEBUG("response client({0}) lics alloc request: vendor({1}), type({2}), algorithm_id({3}), actual_lics({4}), request_id({5}), respcode({6})", 
                request->token(),
                response->algo().vendor(),
                response->algo().type(),
                response->algo().algorithmid(),
                response->clientgetactuallicsnum(),
                response->requestid(),
                response->respcode());  
    return Status::OK;
}

Status LicsServer::deleteLics(const DeleteLicsRequest* request, DeleteLicsResponse* response) {
    SPDLOG_DEBUG("client({0}) send lics free request: vendor({1}), type({2}), algorithm_id({3}), lics({4}), request_id({5})",
                request->token(),
                request->algo().vendor(),
                request->algo().type(),
                request->algo().algorithmid(),
                request->licsnum(),
                request->requestid());
    long clientToken = request->token();
    response->set_token(clientToken);
    response->set_requestid(0);// to be fixed
    response->mutable_algo()->set_vendor(request->algo().vendor());
    response->mutable_algo()->set_type(request->algo().type());
    response->mutable_algo()->set_algorithmid(request->algo().algorithmid());

    int licsNum = licsFree(clientToken, request->algo().algorithmid(), request->licsnum());
    response->set_licsnum(licsNum);
    response->set_respcode(ELICS_OK);

    SPDLOG_DEBUG("response client({0}) lics free request: vendor({1}), type({2}), algorithm_id({3}), lics({4}), request_id({5}), respcode({6})",
                request->token(),
                response->algo().vendor(),
                response->algo().type(),
                response->algo().algorithmid(),
                response->licsnum(),
                response->requestid(),
                response->respcode());
    return Status::OK;                   
}

Status LicsServer::queryLics(const QueryLicsRequest* request, QueryLicsResponse* response) {
    //response->set_total(300);
    return Status::OK;              
}

Status LicsServer::getAuthAccess(const GetAuthAccessRequest* request, GetAuthAccessResponse* response) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);

    SPDLOG_DEBUG("client({0}) send auth access request: ip({1}), port({2})",
                request->token(),
                request->ip(),
                request->port());
    long token = request->token(); // bug to be fixed:: make sure token is 64bits field.

    // TODO: check if token is exist or not, if exist then reallocted a token for client and print error
    auto search = clientQ.find(token);
    if (search != clientQ.end()) {
        SPDLOG_INFO("find a same token client:{0}", token);
    }
    long newToken = newClientToken();
    //SPDLOG_INFO("allocate a new token:{0}", newToken);

    std::map<long, std::shared_ptr<AlgoLics>> algo;
    for (int idx = 0; idx < request->lics_size(); ++idx ) {
        algo[request->lics(idx).algo().algorithmid()] = std::make_shared<AlgoLics>(request->lics(idx));
    }
    std::shared_ptr<Client> c = std::make_shared<Client>(newToken, algo);
    clientQ[newToken] = c;

    response->set_token(newToken);
    response->set_respcode(ELICS_OK);

    SPDLOG_DEBUG("response client(token:{0} ip:{1} port:{2}) auth access request: token({3}), respcode({4})",
                request->token(),
                request->ip(),
                request->port(),
                response->token(),
                response->respcode());

    return Status::OK;             
}

void LicsServer::clientTellServerStillAlive(long token) {
    std::lock_guard<std::mutex> lk(exclusive_write_or_read_server_license);
    auto client = clientQ.find(token);
    if (client == clientQ.end()) {
        SPDLOG_INFO("client({0}) not exist", token);
        return;
    }
    // update client timestamp
    client->second->UpdateTimestamp();
    return;
}

Status LicsServer::keepAlive(const KeepAliveRequest* request, KeepAliveResponse* response) {

    long clientToken = request->token();
    clientTellServerStillAlive(clientToken);

    // TODO: allocte licence for picture
    std::string kp = "client({0}) lics: {1} \nvendor type algorithmID requestID totalLics usedLics clientMaxLimit\n";
    for (int idx = 0; idx < request->lics_size(); ++idx ) {
        if (request->lics(idx).algo().type() == TaskType::PICTURE) {
            int clientMaxLimit = request->lics(idx).maxlimit();
            long algoID = request->lics(idx).algo().algorithmid();

            int clientNum = clientNumByAlgoID(algoID);
            int totalLics = 0;
            int usedLics = 0;
            licsQuery(clientToken, algoID, totalLics,usedLics);
            int average = clientNum > 0 ? (totalLics / clientNum) : totalLics;
            int clientFetchLics = average > clientMaxLimit ? clientMaxLimit : average;
            response->add_lics()->set_totallics(clientFetchLics);
        }
        kp += std::to_string(request->lics(idx).algo().vendor()) + "\t" + 
                std::to_string(request->lics(idx).algo().type()) + "\t" +
                std::to_string(request->lics(idx).algo().algorithmid()) + "\t" +
                std::to_string(request->lics(idx).requestid()) + "\t\t" + 
                std::to_string(request->lics(idx).totallics()) + "\t" + 
                std::to_string(request->lics(idx).usedlics()) + "\t" + 
                std::to_string(request->lics(idx).maxlimit()) + "\n";
    }
    SPDLOG_DEBUG(kp, clientToken, request->lics_size());
    response->set_token(clientToken);
    response->set_respcode(ELICS_OK);
    return Status::OK;             
}

Status LicsServer::CreateLics(ServerContext* context, 
                const CreateLicsRequest* request, 
                CreateLicsResponse* response) {
    return createLics(request, response);
}


Status LicsServer::DeleteLics(ServerContext* context, 
                const DeleteLicsRequest* request, 
                DeleteLicsResponse* response) {
    return deleteLics(request, response);
}

Status LicsServer::QueryLics(ServerContext* context, 
                const QueryLicsRequest* request, 
                QueryLicsResponse* response) {
    return queryLics(request, response);
}

Status LicsServer::GetAuthAccess(ServerContext* context, 
            const GetAuthAccessRequest* request, 
            GetAuthAccessResponse* response) {
    return getAuthAccess(request, response);
}

Status LicsServer::KeepAlive(ServerContext* context, 
            const KeepAliveRequest* request, 
            KeepAliveResponse* response) {
    return keepAlive(request, response);
}


void RunServer() {
    std::string server_address("0.0.0.0:" + getServerConf()->GetItem("port")); 
    LicsServer service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    ServerBuilder builder;
    // Listen on the given address without any authentication mechanism.
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    // Register "service" as the instance through which we'll communicate with
    // clients. In this case it corresponds to an *synchronous* service.
    builder.RegisterService(&service);
    // Finally assemble the server.
    std::unique_ptr<Server> server(builder.BuildAndStart());
    SPDLOG_INFO("Server listening on {0}", server_address);

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}