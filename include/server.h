

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

#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/rotating_file_sink.h"

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
    void IncFailedCnt();
    void ClearFailedCnt();

private:
    long clientToken {-1};
    long timestamp{0};
    int continusKeepAliveFailedCnt {0};
    std::map<long, std::shared_ptr<AlgoLics>> algo; // key is algorithm id
};



#endif