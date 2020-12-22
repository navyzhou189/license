#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>
#include "license.grpc.pb.h"
#include "lics_error.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using UnisAlgoLics::CreateLicsRequest;
using UnisAlgoLics::CreateLicsResponse;
using UnisAlgoLics::DeleteLicsRequest;
using UnisAlgoLics::DeleteLicsResponse;
using UnisAlgoLics::QueryLicsRequest;
using UnisAlgoLics::QueryLicsResponse;
using UnisAlgoLics::GetAuthAccessRequest;
using UnisAlgoLics::GetAuthAccessResponse;
using UnisAlgoLics::License;


class LicsClient {
public:
    LicsClient(std::shared_ptr<Channel> channel)
        : stub_(License::NewStub(channel)) {}

    int CreateLics(){
        if (!proxyConnected_) {
            std::cout << "proxy is offline, connecting to proxy..." << std::endl;
            if (!getAuthAcess()) {
                std::cout << "failed to connect to proxy: CreateLics" << std::endl;
                return ELICS_NOK;
            }
            std::cout << "connected to proxy: ok" << std::endl;
        }

        // Data we are sending to the server.
        CreateLicsRequest req;

        // Container for the data we expect from the server.
        CreateLicsResponse resp;

        // Context for the client. It could be used to convey extra information to
        // the server and/or tweak certain RPC behaviors.
        ClientContext context;

        // The actual RPC.
        Status status = stub_->CreateLics(&context, req, &resp);

        // Act upon its status.
        if (status.ok()) {
            return ELICS_OK;
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                        << std::endl;
            return ELICS_NOK;
        }
    }

    int DeleteLics() {
        if (!proxyConnected_) {
            std::cout << "proxy is offline, connecting to proxy..." << std::endl;
            if (!getAuthAcess()) {
                std::cout << "failed to connect to proxy: DeleteLics" << std::endl;
                return ELICS_NOK;
            }
            std::cout << "connected to proxy: ok" << std::endl;
        }

        DeleteLicsRequest req;
        DeleteLicsResponse resp;
        ClientContext context;
        Status status = stub_->DeleteLics(&context, req, &resp);
        if (status.ok()) {
            return ELICS_OK;
        } else {
            std::cout << status.error_code() << ": " << status.error_message()
                        << std::endl;
            return ELICS_NOK;
        }

    }

    void QueryLics() {
        if (!proxyConnected_) {
            std::cout << "proxy is offline, connecting to proxy..." << std::endl;
            if (!getAuthAcess()) {
                std::cout << "failed to connect to proxy: DeleteLics" << std::endl;
                return;
            }
            std::cout << "connected to proxy: ok" << std::endl;
        }

        QueryLicsRequest req;
        QueryLicsResponse resp;
        ClientContext context;
        Status status = stub_->QueryLics(&context, req, &resp);
    }

private:
    bool getAuthAcess() {
        GetAuthAccessRequest req;
        GetAuthAccessResponse resp;
        ClientContext context;
        Status status = stub_->GetAuthAccess(&context, req, &resp);
        if (status.ok()) {
            return true;
        }

        return false;
    }

private:
    std::unique_ptr<License::Stub> stub_;

    bool proxyConnected_ {false}; // proxy receving client request.
};

int main(int argc, char** argv)
{
    std::string remote("localhost:50057"); // TODO: load from conf file.
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));
    int resp = client.CreateLics();
    std::cout << "client CreateLics " << resp << std::endl;

    return 0;
}