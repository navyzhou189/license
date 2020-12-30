#include "client.h"

int main(int argc, char** argv)
{
    std::string remote("localhost:50057"); // TODO: load from conf file.
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest req;
    req.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    req.mutable_algo()->set_type(TaskType::VIDEO);
    req.mutable_algo()->set_algorithmid(UNIS_OD);

    // Container for the data we expect from the server.
    CreateLicsResponse resp;

    int ret = client.CreateLics(req, resp);
    std::cout << "client CreateLics: " << ret << std::endl;

    return 0;
}