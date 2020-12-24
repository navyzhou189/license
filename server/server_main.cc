
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <map>

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "license.grpc.pb.h"
#include "lics_error.h"

#define SERVER_CONF_FILE   ("/var/unis/license/conf/server.conf")

using grpc::Server;
using grpc::Channel;
using grpc::ServerContext;
using grpc::ServerBuilder;
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


class LicsServer final : public License::Service {
public:
LicsServer();
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
    long newToken();

private:
    long tokenBase_{0};// TODO:: lock contention
};

LicsServer::LicsServer() {
    
}

long LicsServer::newToken() {

    // TODO: add lock
    tokenBase_++;
    return tokenBase_;
}

Status LicsServer::CreateLics(ServerContext* context, 
                const CreateLicsRequest* request, 
                CreateLicsResponse* response) {
    // TODO: just for test
    // response->set_taskid(100);
    return Status::OK;
}


Status LicsServer::DeleteLics(ServerContext* context, 
                const DeleteLicsRequest* request, 
                DeleteLicsResponse* response) {
    //response->set_taskid(200);
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
    response->set_token(newToken());

    return Status::OK;
}

Status LicsServer::KeepAlive(ServerContext* context, 
            const KeepAliveRequest* request, 
            KeepAliveResponse* response) {
    //response->set_taskid(400);
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
            std::cout << "failed to open file:" << file << std::endl;
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
    std::string server_address("0.0.0.0:" + port); // TODO: put server address into a config file.
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
    std::cout << "Server listening on " << server_address << std::endl;

    // Wait for the server to shutdown. Note that some other thread must be
    // responsible for shutting down the server for this call to ever return.
    server->Wait();
}

int main(int argc, char** argv)
{
    ServerConf conf(SERVER_CONF_FILE);

    std::string port = conf.GetItem("port");
    RunServer(port);

    return 0;
}