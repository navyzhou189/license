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

long GetTimeSecsFromEpoch() {
    std::time_t result = std::time(nullptr);
    return result;
}

Client::Client(long token) : clientToken(token) {
    timestamp = GetTimeSecsFromEpoch();
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

bool Client::Alive() {

    // TODO: read max keep alive from conf
    if (continusKeepAliveFailedCnt > 3) {
        return false;
    }

    return true;
}

void Client::AddUsedLics(long algoID, int num) {
    auto search = algo.find(algoID);
    if (search == algo.end()) {
        return;
    }

    int used = search->second->usedlics();
    search->second->set_usedlics(used + num);
}

void Client::DecUsedLics(long algoID, int num) {
    auto search = algo.find(algoID);
    if (search == algo.end()) {
        return;
    }

    int used = search->second->usedlics();
    search->second->set_usedlics(used - num);
}

LicsServer::LicsServer() {

    // load all license into cache, depend by vendor
    std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
    odLics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    odLics->mutable_algo()->set_type(TaskType::VIDEO);
    odLics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
    odLics->set_requestid(-1);
    odLics->set_totallics(0);
    odLics->set_usedlics(0);
    odLics->set_maxlimit(0); // TODO: set by app
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

void LicsServer::Shutdown() {
    running_ = false;
    // TODO: sleep for a delay to shutdown server.
}


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

#define FETCH_ALGOS_TOTAL_LICS_URL  {\
std::string("http://" + getServerConf()->GetItem("cloud") + "/api/vcloud/v2/license/authinfos")\
}

void LicsServer::fetchAlgosTotalLicFromCloud(std::map<long, std::shared_ptr<AlgoLics>>& remote){
    std::string reply;
    if (getHttpClient()->Get(FETCH_ALGOS_TOTAL_LICS_URL, reply) != EHTTP_OK)
        return;

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
    // TODO: add lock
    for (const auto &remote : remoteAlgosTotalLic) {
        licenseQ[remote.first]->set_totallics(remote.second->totallics());
    }
}

void LicsServer::getLocalLics(std::map<long, std::shared_ptr<AlgoLics>>& local) {

}

void LicsServer::doLoop() {
    while (running_) {

        long sysTime = GetTimeSecsFromEpoch();// prevent from mulitiple call in the loop
        for (auto client = clientQ.begin(); client != clientQ.end();) {
             // check if the client keep alive
            long clientTime = client->second->GetLatestTimestamp();
            
            long diff = sysTime >= clientTime ? sysTime - clientTime : 0; // TODO: mark the client died if sysTime < clientTime.
            // TODO: get timeout from conf
            if (diff > 30) {
                // make the continusKeepAliveFailedCnt plus plus
                client->second->IncHeartbeatTimeoutCnt();
                SPDLOG_INFO("client({0}) hearbeat timeout reach {1}", client->first, client->second->HeartbeatTimeoutCnt());
            }

            if (!client->second->Alive()) {
                // TODO:delete client and release used lics
                SPDLOG_INFO("detect heatbeat-stoped client. remove token:{0}", client->second->GetToken());
                client = clientQ.erase(client);
            } else {
                ++client;
            }
        }

        // TODO: call vcloud api to update license.
        std::map<long, std::shared_ptr<AlgoLics>> remoteAlgosTotalLic;
        fetchAlgosTotalLicFromCloud(remoteAlgosTotalLic);

        updateLocalLics(remoteAlgosTotalLic);

        std::map<long, std::shared_ptr<AlgoLics>> cacheAlgosUsedLic;
        getLocalLics(cacheAlgosUsedLic);

        pushAlgosUsedLicToCloud(cacheAlgosUsedLic);
        
        // TODO: get interval from conf
        sleep(30);

    }
}

int LicsServer::licsAlloc(long token, long algoID, int expected) {

    // add lock
    auto client = clientQ.find(token); // search client
    if (client == clientQ.end()) {
        // TODO: add log print
        return 0;
    }

    auto algo = licenseQ.find(algoID);
    if (algo == licenseQ.end()) {
        // TODO: add log print
        return 0;
    }

    // get total and used lics by algorithm id
    int total = algo->second->totallics();
    int used = algo->second->usedlics();

    int actualAllocedLics = (total - used) >= expected ? expected : (total - used);
    algo->second->set_usedlics(used + actualAllocedLics);// update used licenses for algorithm
    client->second->AddUsedLics(algoID, actualAllocedLics); // update used licenses for client

    return actualAllocedLics;
}

int LicsServer::licsFree(long token, long algoID, int expected) { 

    // add lock
    auto client = clientQ.find(token); // search client
    if (client == clientQ.end()) {
        // TODO: add log print
        return 0;
    }

    auto algo = licenseQ.find(algoID);
    if (algo == licenseQ.end()) {
        // TODO: add log print
        return 0;
    }

    int used = algo->second->usedlics();

    int actualFreeLics = used >= expected ? expected : used;
    algo->second->set_usedlics(used - actualFreeLics);// update used licenses for algorithm
    client->second->DecUsedLics(algoID, actualFreeLics); // update used licenses for client

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
    auto client = clientQ.find(clientToken);
    if (client == clientQ.end()) {
        response->set_respcode(ELICS_CLIENT_NOT_EXIST);
        return Status::OK;
    }

    response->set_token(clientToken);
    response->set_requestid(0);// to be fixed
    response->mutable_algo()->set_vendor(request->algo().vendor());
    response->mutable_algo()->set_type(request->algo().type());
    response->mutable_algo()->set_algorithmid(request->algo().algorithmid());

    // TODO: add lock
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
    auto client = clientQ.find(clientToken);
    if (client == clientQ.end()) {
        response->set_respcode(ELICS_CLIENT_NOT_EXIST);
        return Status::OK;
    }

    response->set_token(clientToken);
    response->set_requestid(0);// to be fixed
    response->mutable_algo()->set_vendor(request->algo().vendor());
    response->mutable_algo()->set_type(request->algo().type());
    response->mutable_algo()->set_algorithmid(request->algo().algorithmid());

    // TODO: add lock
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
    SPDLOG_DEBUG("client({0}) send auth access request: ip({1}), port({2})",
                request->token(),
                request->ip(),
                request->port());
    long token = request->token(); // bug to be fixed:: make sure token is 64bits field.

    // TODO: check if token is exist or not, if exist then reallocted a token for client and print error
    // TODO: add lock
    auto search = clientQ.find(token);
    if (search != clientQ.end()) {
        SPDLOG_INFO("find a same token client:{0}", token);
    }
    long newToken = newClientToken();
    //SPDLOG_INFO("allocate a new token:{0}", newToken);

    std::shared_ptr<Client> c = std::make_shared<Client>(newToken);
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

Status LicsServer::keepAlive(const KeepAliveRequest* request, 
            KeepAliveResponse* response) {
    // check if client exist
    long clientToken = request->token();

    response->set_token(clientToken);

    // TODO: add lock
    auto client = clientQ.find(clientToken);
    if (client == clientQ.end()) {
        response->set_respcode(ELICS_CLIENT_NOT_EXIST);
        SPDLOG_INFO("keepalive failed: client({0}) not exist", clientToken);
        return Status::OK;
    }

    // TODO: allocte licence for picture
    std::string kp = "client({0}) lics: {1} \nvendor type algorithmID requestID totalLics usedLics clientMaxLimit\n";
    for (int idx = 0; idx < request->lics_size(); ++idx ) {
        if (request->lics(idx).algo().type() == TaskType::PICTURE) {
            int clientMaxLimit = request->lics(idx).maxlimit();

            // TODO: allocate stragy
            response->add_lics()->set_maxlimit(clientMaxLimit);
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

    // update client timestamp
    client->second->UpdateTimestamp();

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