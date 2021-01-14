

#ifndef LICENSE_SERVER_HH
#define LICENSE_SERVER_HH

#include <map>
#include <iostream>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <list>
#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>

#include "license.grpc.pb.h"
#include "lics_error.h"
#include "utils.h"
#include "lics_interface.h"


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

enum LicsServerEventType {
    EXIT = 0,
};

class LicsServerEvent {
public:
    LicsServerEvent(LicsServerEventType type) : type_(type) {}

    LicsServerEventType GetEventType() { return type_; }

private:
    LicsServerEventType type_;
};

class Client {

public:
    Client(long token, std::map<long, std::shared_ptr<AlgoLics>> algo);
    void AddLics(long algoID, int num);
    void DecLics(long algoID, int num);
    long GetToken();
    long GetLatestTimestamp();
    bool Alive();
    void UpdateTimestamp();
    int HeartbeatTimeoutCnt();
    void IncHeartbeatTimeoutCnt();
    void ZeroHeartbeatTimeoutCnt();
    bool HaveAlgoID(long algoID);
    std::map<long, std::shared_ptr<AlgoLics>> Algos();

private:
    long clientToken {-1};
    long timestamp{0};
    int continusKeepAliveFailedCnt {0};
    std::map<long, std::shared_ptr<AlgoLics>> algo; // key is algorithm id
};

class LicsServer : public License::Service {
public:
LicsServer();
~LicsServer();

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

    void signalExit();
    std::shared_ptr<LicsServerEvent> dequeue();
    void enqueue(std::shared_ptr<LicsServerEvent> t);
    bool empty();
    bool gotExitSignal(std::shared_ptr<LicsServerEvent> t);
    void serverClearDeadClients();
    void clientTellServerStillAlive(long token);

    void print();

protected:
    // TEST-Class call following functions to verify data correct.
    Status createLics(const CreateLicsRequest* request, CreateLicsResponse* response);
    Status deleteLics(const DeleteLicsRequest* request, DeleteLicsResponse* response);
    Status queryLics(const QueryLicsRequest* request, QueryLicsResponse* response);
    Status getAuthAccess(const GetAuthAccessRequest* request,  GetAuthAccessResponse* response);
    Status keepAlive(const KeepAliveRequest* request, KeepAliveResponse* response);
    void licsQuery(long token, long algoID, int& total, int& used);
    int totalClientNum();
    int clientNumByAlgoID(long algoID);

    // TEST-Class will override the following methods.
    virtual void updateLocalLics(const std::map<long, std::shared_ptr<AlgoLics>>& remote);
    virtual void getLocalLics(std::map<long, std::shared_ptr<AlgoLics>>& local);
    virtual void pushAlgosUsedLicToCloud(const std::map<long, std::shared_ptr<AlgoLics>>& local);
    virtual void fetchAlgosTotalLicFromCloud(std::map<long, std::shared_ptr<AlgoLics>>& remote);

private:
    std::atomic<long> tokenBase_{0};
    std::map<long,std::shared_ptr<Client>> clientQ; // key is user token.
    std::map<long, std::shared_ptr<AlgoLics>> licenseQ; // key is algorithm id.
    std::mutex exclusive_write_or_read_server_license; // used to prevent multiple thread read or write licenseQ and clientQ
    std::atomic<bool> running_{true};

    std::list<std::shared_ptr<LicsServerEvent>> event_;
    std::mutex exclusive_write_or_read_event;
    std::condition_variable cv_of_event_;
};

void RunServer();


#endif