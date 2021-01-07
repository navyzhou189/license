

#ifndef LICENSE_SERVER_HH
#define LICENSE_SERVER_HH

#include <map>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <thread>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "license.grpc.pb.h"
#include "lics_error.h"
#include "http.h"


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
using UnisAlgoLics::Algorithm;
using UnisAlgoLics::TaskType;
using UnisAlgoLics::AlgoLics;
using UnisAlgoLics::Vendor;

class Client {

public:
    Client(long token);
    void AddUsedLics(long algoID, int num);
    void DecUsedLics(long algoID, int num);
    long GetToken();
    long GetLatestTimestamp();
    bool Alive();
    void UpdateTimestamp();
    int HeartbeatTimeoutCnt();
    void IncHeartbeatTimeoutCnt();
    void ZeroHeartbeatTimeoutCnt();

private:
    long clientToken {-1};
    long timestamp{0};
    int continusKeepAliveFailedCnt {0};
    std::map<long, std::shared_ptr<AlgoLics>> algo; // key is algorithm id
};

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
    HttpClient httpClient;
};



#endif