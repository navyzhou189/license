#include "server.h"

#define SERVER_CONF_FILE   ("/var/unis/license/server/conf/server.conf")

Client::Client(long token) : clientToken(token) {

}

void Client::UpdateTimestamp() {
    // TODO: update timestamp with system;
}

void Client::IncFailedCnt() {
    ++continusKeepAliveFailedCnt;
}

void Client::ClearFailedCnt() {
    continusKeepAliveFailedCnt = 0;
}

long Client::GetLatestTimestamp() {
    return timestamp;
}

bool Client::Alive() {

    // TODO: read max keep alive from conf
    if (continusKeepAliveFailedCnt) {
        return true;
    }

    return false;
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

        for (auto& client : clientQ) {
            if (!client.second->Alive()) {
                // delete client and release used lics
            }

            // check if the client keep alive
            long clientTime = client.second->GetLatestTimestamp();
            long sysTime = 0;// bug to be fixed
            long diff = sysTime >= clientTime ? sysTime - clientTime : 0; // TODO: mark the client died if sysTime < clientTime.
            // TODO: get timeout from conf
            if (diff > 30) {
                // make the continusKeepAliveFailedCnt plus plus
                client.second->IncFailedCnt();
            }
        }


        // TODO: call vcloud api to update license.


        // TODO: get interval from conf
        sleep(1);

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
    
    return Status::OK;
}


Status LicsServer::DeleteLics(ServerContext* context, 
                const DeleteLicsRequest* request, 
                DeleteLicsResponse* response) {
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
    long token = request->token(); // bug to be fixed:: make sure token is 64bits field.

    // TODO: check if token is exist or not, if exist then reallocted a token for client and print error
    // TODO: add lock
    auto search = clientQ.find(token);
    if (search != clientQ.end()) {
        SPDLOG_INFO("find a same token client:{0}", token);
    }
    long newToken = newClientToken();
    SPDLOG_INFO("allocate a new token:{0}", newToken);

    std::shared_ptr<Client> c = std::make_shared<Client>(newToken);
    clientQ[newToken] = c;

    response->set_token(newToken);
    response->set_respcode(ELICS_OK);

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
        return Status::OK;
    }

    // TODO: allocte licence for picture
    for (int idx = 0; idx < request->lics_size(); ++idx ) {
        if (request->lics(idx).algo().type() == TaskType::PICTURE) {
            int clientMaxLimit = request->lics(idx).maxlimit();

            // TODO: allocate stragy
            response->add_lics()->set_maxlimit(clientMaxLimit);
        }
    }

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
    log->flush_on(spdlog::level::info); //set flush policy 
    spdlog::set_default_logger(log); // set log to be defalut 
    spdlog::set_pattern("%Y-%m-%d %H:%M:%S.%e %l [%s:%!:%#] %v");    

    std::string port = conf.GetItem("port");
    RunServer(port);

    return 0;
}