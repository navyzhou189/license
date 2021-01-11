#include "client.h"

#include "gtest/gtest.h"
#include <iostream>

int lics_global_init_internal(const char* remote, AlgoCapability* algoLics, int size, std::shared_ptr<LicsClient> client);

class LicsClientStub : public LicsClient {
public:
    LicsClientStub(std::shared_ptr<Channel> channel, AlgoCapability* algoLics, int size) : LicsClient{channel, algoLics, size} {}
    Status createLics(CreateLicsRequest& req, CreateLicsResponse& resp) override {
        return Status();
    }
    Status deleteLics(DeleteLicsRequest& req, DeleteLicsResponse& resp) override {
        return Status();
    }

    Status getAuthAccess(const GetAuthAccessRequest& req, GetAuthAccessResponse& resp) override {
        resp.set_token(16888);
        return Status();
    }
    Status keepAlive(const KeepAliveRequest& req, KeepAliveResponse& resp) override {
        return Status();
    }
};

class LicsServerTests : public testing::Test {

public:
    void SetUp() override {
        char remote[] = "localhost:50057";
        AlgoCapability cap[2];
        cap[0].algoID = UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD;
        cap[0].maxLimit = 16;
        cap[0].type = AlgoLicsType::VIDEO;

        cap[1].algoID = UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA;
        cap[1].maxLimit = 100;
        cap[1].type = AlgoLicsType::PICTURE;

        int size = 2;
        std::shared_ptr<LicsClient>  clientStub = std::make_shared<LicsClientStub>(
            grpc::CreateChannel(remote, grpc::InsecureChannelCredentials()),
            cap, 
            size);


        int ret = lics_global_init_internal(remote, cap, 2, clientStub);
        ASSERT_EQ(ret, ELICS_OK);
    }

    void TearDown() {
        lics_global_cleanup();
    }

};


// int lics_apply(int algoID, const int expectLicsNum, int* actualLicsNum);

// int lics_free(int algoID, const int licsNum);


// const char* lics_version();


TEST_F(LicsServerTests,LicsApplyShouldReturnOk) {

    int actualLicsNum = 0;
    sleep(1);

    int ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, 10, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);

    

    ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, 10, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);


}

// TEST_F(LicsServerTests, LicsApplyShouldReturnNok) {

// }

// TEST_F(LicsServerTests, LicsFreeShouldReturnOk) {

// }

// TEST_F(LicsServerTests, LicsFreeShouldReturnNok) {

// }

TEST(LicsVersion, ShouldRetrunOk) {

}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}