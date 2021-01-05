#include "client.h"

#include "gtest/gtest.h"

#if 0

TEST(Client, Keepalive) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest createReq;
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::VIDEO);
    createReq.mutable_algo()->set_algorithmid(UNIS_OD);
    createReq.set_clientexpectedlicsnum(1);

    // Container for the data we expect from the server.
    CreateLicsResponse createResp;

    int createTestRet = client.CreateLics(createReq, createResp);
    ASSERT_EQ(createTestRet, ELICS_OK);

    sleep(36000);// client back-thread will do keepavlie, we just sleep here.

}


TEST(Video, CreateAndDeleteOdLics) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest createReq;
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::VIDEO);
    createReq.mutable_algo()->set_algorithmid(UNIS_OD);
    createReq.set_clientexpectedlicsnum(1);

    // Container for the data we expect from the server.
    CreateLicsResponse createResp;

    int createTestRet = client.CreateLics(createReq, createResp);
    ASSERT_EQ(createTestRet, ELICS_OK);

    DeleteLicsRequest deleteReq;
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(TaskType::VIDEO);
    deleteReq.mutable_algo()->set_algorithmid(UNIS_OD);
    deleteReq.set_token(createResp.token());
    deleteReq.set_licsnum(createResp.clientgetactuallicsnum());
    DeleteLicsResponse deleteResp;
    int deleteTestRet = client.DeleteLics(deleteReq, deleteResp);
    ASSERT_EQ(deleteTestRet, ELICS_OK);

}

TEST(Video, CreateInvalidOdLics) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest req;
    req.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    req.mutable_algo()->set_type(TaskType::VIDEO);
    req.mutable_algo()->set_algorithmid(999999); // 999999 is a invalid algorithm id.

    // Container for the data we expect from the server.
    CreateLicsResponse resp;

    int ret = client.CreateLics(req, resp);
    ASSERT_EQ(ret, ELICS_ALGO_NOT_EXIST);
}


TEST(Video, DeleteInvalidOdLics) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    CreateLicsRequest createReq;
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::VIDEO);
    createReq.mutable_algo()->set_algorithmid(UNIS_OD);
    CreateLicsResponse createResp;
    int createTestRet = client.CreateLics(createReq, createResp);
    ASSERT_EQ(createTestRet, ELICS_OK);

    sleep(1);

    DeleteLicsRequest deleteReq;
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(TaskType::VIDEO);
    deleteReq.mutable_algo()->set_algorithmid(999999);
    deleteReq.set_token(createResp.token());
    deleteReq.set_licsnum(createResp.clientgetactuallicsnum());
    DeleteLicsResponse deleteResp;
    int deleteTestRet = client.DeleteLics(deleteReq, deleteResp);
    ASSERT_EQ(deleteTestRet, ELICS_ALGO_NOT_EXIST);
}

TEST(Video, InvalidToken) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    DeleteLicsRequest deleteReq;
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(TaskType::VIDEO);
    deleteReq.mutable_algo()->set_algorithmid(999999);
    deleteReq.set_token(-256); // token(-256) is not exist
    deleteReq.set_licsnum(1);
    DeleteLicsResponse deleteResp;
    int deleteTestRet = client.DeleteLics(deleteReq, deleteResp);
    ASSERT_EQ(deleteTestRet, ELICS_ALGO_NOT_EXIST);
}

TEST(Picture, CreateAndDeleteFaceOaLics) {

}

TEST(Picture, CreateInvalidFaceOaLics) {

}
#endif

TEST(Picture, CreateFaceOA) {
    std::string remote("localhost:50057"); 
    LicsClient client(grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()));

    // Data we are sending to the server.
    CreateLicsRequest createReq;
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::PICTURE);
    createReq.mutable_algo()->set_algorithmid(UNIS_FACE_OA);
    createReq.set_clientexpectedlicsnum(1);

    // Container for the data we expect from the server.
    CreateLicsResponse createResp;

    int createTestRet = client.CreateLics(createReq, createResp);
    ASSERT_EQ(createTestRet, ELICS_OK);
    ASSERT_EQ(createResp.clientgetactuallicsnum(), 1);

    sleep(5);

    DeleteLicsRequest deleteReq;
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(TaskType::PICTURE);
    deleteReq.mutable_algo()->set_algorithmid(UNIS_FACE_OA);
    deleteReq.set_token(createResp.token());
    deleteReq.set_licsnum(createResp.clientgetactuallicsnum());
    DeleteLicsResponse deleteResp;
    int deleteTestRet = client.DeleteLics(deleteReq, deleteResp);
    ASSERT_EQ(deleteTestRet, ELICS_OK);
}



int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}