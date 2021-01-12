#include "server.h"

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

void SetLog() {

}

int main(int argc, char** argv)
{
    SetLog();
    RunServer();

    return 0;
}