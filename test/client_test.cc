#include "client.h"

#include "gtest/gtest.h"
#include <iostream>

#define USER_TOKEN  (16888)
#define MAX_LICS_NUM    (10)

int lics_global_init_internal(const char* remote, AlgoCapability* algoLics, int size, std::shared_ptr<LicsClient> client);

class LicsClientStub : public LicsClient {
public:
    LicsClientStub(std::shared_ptr<Channel> channel, AlgoCapability* algoLics, int size) : LicsClient{channel, algoLics, size} {}
    Status createLics(CreateLicsRequest& req, CreateLicsResponse& resp) override {

        int expectedNum = req.clientexpectedlicsnum();
        if (expectedNum <= MAX_LICS_NUM) {
            resp.set_clientgetactuallicsnum(expectedNum);
        }else {
            resp.set_clientgetactuallicsnum(MAX_LICS_NUM);
        }
     
        resp.set_respcode(0);

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

        for (int idx = 0; idx < req.lics_size(); ++idx ) {
            if (req.lics(idx).algo().type() == TaskType::PICTURE) {
                
                int clientMaxLimit = req.lics(idx).maxlimit();

                AlgoLics* lics = resp.add_lics();
                lics->set_maxlimit(clientMaxLimit);
                if (clientMaxLimit <= MAX_LICS_NUM) {
                    lics->set_totallics(clientMaxLimit);
                }else {
                    lics->set_totallics(MAX_LICS_NUM);
                }
                lics->set_requestid(req.lics(idx).requestid());
                lics->set_usedlics(req.lics(idx).usedlics());
                lics->mutable_algo()->set_vendor(req.lics(idx).algo().vendor());
                lics->mutable_algo()->set_type(req.lics(idx).algo().type());
                lics->mutable_algo()->set_algorithmid(req.lics(idx).algo().algorithmid());
            }
        }
     
        resp.set_respcode(0);

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


// const char* lics_version();


TEST_F(LicsServerTests,LicsApplyODShouldReturn1) {

    int actualLicsNum = 0;
    int expctedNum = 1;

    int ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, expctedNum, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);
    EXPECT_EQ(actualLicsNum, expctedNum);

}

TEST_F(LicsServerTests, LicsApplyODShouldReturn10) {
    int actualLicsNum = 0;
    int expctedNum = 100;

    int ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, expctedNum, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);
    EXPECT_EQ(actualLicsNum, 10);
}

TEST_F(LicsServerTests,LicsApplyOAShouldReturn1) {

    sleep(1);// make enough time to sync picture with server.

    int actualLicsNum = 0;
    int expctedNum = 1;

    int ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA, expctedNum, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);
    EXPECT_EQ(actualLicsNum, expctedNum);

}


TEST_F(LicsServerTests, LicsApplyOAShouldReturn10) {

    sleep(1);// make enough time to sync picture with server.
    
    int actualLicsNum = 0;
    int expctedNum = 100;

    int ret = lics_apply(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA, expctedNum, &actualLicsNum);
    EXPECT_EQ(ret, ELICS_OK);
    EXPECT_EQ(actualLicsNum, 10);
}

TEST_F(LicsServerTests, LicsFreeODShouldReturnOk) {
    int licsNum = 1;
    int ret = lics_free(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, licsNum);
    EXPECT_EQ(ret, ELICS_OK);
}

TEST_F(LicsServerTests, LicsFreeOAShouldReturnNok) {
    int licsNum = 1;
    int ret = lics_free(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA, licsNum);
    EXPECT_EQ(ret, ELICS_OK);
}

TEST(LicsVersion, ShouldRetrunOk) {

}


int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}