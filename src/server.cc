#include "server.h"
#include <ctime>

long GetTimeSecsFromEpoch() {
    std::time_t result = std::time(nullptr);
    return result;
}


#define SPDLOG_ACTIVE_LEVEL SPDLOG_LEVEL_DEBUG // set level beyond debug when put it into production 
#include "spdlog/spdlog.h"
#include "spdlog/fmt/ostr.h" // must be included if log user defined object
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

#define SERVER_CONF_FILE   ("/var/unis/license/server/conf/server.conf")

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

class LicsServer final : public License::Service {
public:
LicsServer();

void Shutdown();

Status CreateLics(ServerContext* context, 
                const CreateLicsRequest* request, 
                CreateLicsResponse* response) override;
Status DeleteLics(ServerContext* context, 
                const DeleteLicsRequest* request, 
                DeleteLicsResponse* response) override;
Status QueryLics(ServerContext* context, 
                const QueryLicsRequest* request, 
                QueryLicsResponse* response) override;
Status GetAuthAccess(ServerContext* context, 
            const GetAuthAccessRequest* request, 
            GetAuthAccessResponse* response) override;
Status KeepAlive(ServerContext* context, 
            const KeepAliveRequest* request, 
            KeepAliveResponse* response) override;

private:
    long newClientToken();
    int licsAlloc(long token, long algoID, int expected);
    int licsFree(long token, long algoID, int expected);
    void doLoop();

private:
    long tokenBase_{0};// TODO:: lock contention
    std::map<long,std::shared_ptr<Client>> clientQ; // key is user token.
    std::map<long, std::shared_ptr<AlgoLics>> licenseQ; // key is algorithm id.
    std::atomic<bool> running_{true};
};

LicsServer::LicsServer() {
    std::thread t(&LicsServer::doLoop, this);
    t.detach();
}

void LicsServer::Shutdown() {
    running_ = false;
    // TODO: sleep for a delay to shutdown server.
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

Status LicsServer::CreateLics(ServerContext* context, 
                const CreateLicsRequest* request, 
                CreateLicsResponse* response) {
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


Status LicsServer::DeleteLics(ServerContext* context, 
                const DeleteLicsRequest* request, 
                DeleteLicsResponse* response) {
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

Status LicsServer::QueryLics(ServerContext* context, 
                const QueryLicsRequest* request, 
                QueryLicsResponse* response) {
    //response->set_total(300);
    return Status::OK;
}

Status LicsServer::GetAuthAccess(ServerContext* context, 
            const GetAuthAccessRequest* request, 
            GetAuthAccessResponse* response) {
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

Status LicsServer::KeepAlive(ServerContext* context, 
            const KeepAliveRequest* request, 
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


void RunServer(const std::string& port) {
    std::string server_address("0.0.0.0:" + port); 
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

int main(int argc, char** argv)
{
    ServerConf conf(SERVER_CONF_FILE);

    auto log = spdlog::rotating_logger_mt("server", conf.GetItem("log"), 1048576 * 5, 3);
    log->flush_on(spdlog::level::debug); //set flush policy 
    spdlog::set_default_logger(log); // set log to be defalut 
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l [%s:%!:%#] %v");   
    spdlog::set_level(spdlog::level::debug); 

    std::string port = conf.GetItem("port");
    RunServer(port);

    return 0;
}