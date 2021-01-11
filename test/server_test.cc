#include "server.h"

#include "gtest/gtest.h"

#define TEST_MAX_OA_LICS_NUM    (100000)
#define TEST_MAX_OD_LICS_NUM    (100000)

class LicsServerTests : public testing::Test, public LicsServer {
    // virtual void SetUp() will be called before each test is run.  You
    // should define it if you need to initialize the variables.
    // Otherwise, this can be skipped.
    void SetUp() override {

    }

    // virtual void TearDown() will be called after each test is run.
    // You should define it if there is cleanup work to do.  Otherwise,
    // you don't have to provide it.
    //
    void TearDown() {

    }


    void pushAlgosUsedLicToCloud(const std::map<long, std::shared_ptr<AlgoLics>>& local) {
        
    }

    void fetchAlgosTotalLicFromCloud(std::map<long, std::shared_ptr<AlgoLics>>& remote) {

        std::shared_ptr<AlgoLics> oaLics = std::make_shared<AlgoLics>();
        oaLics->set_totallics(TEST_MAX_OA_LICS_NUM);

        std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
        odLics->set_totallics(TEST_MAX_OD_LICS_NUM);

        remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA] = oaLics;
        remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD] = odLics;
    }


};


// interface tests
TEST_F(LicsServerTests, AuthShouldOk) {

}

TEST_F(LicsServerTests, AuthShouldFail) {
  // You can access data in the test fixture here.
  GetAuthAccessRequest request;
  GetAuthAccessResponse response;

  Status ret = getAuthAccess(&request,  &response);
  EXPECT_FALSE(ret.ok());

}

TEST_F(LicsServerTests, CreateOdLicsShouldOk) {

}

TEST_F(LicsServerTests, CreateOdLicsShouldFail) {

}

TEST_F(LicsServerTests, DeleteOdLicsShouldOk) {

}

TEST_F(LicsServerTests, DeleteOdLicsShouldFail) {

}

TEST_F(LicsServerTests,KeepAliveShouldOk) {

}

TEST_F(LicsServerTests, KeepAliveShouldFail) {

}

// performance tests
TEST_F(LicsServerTests, 1000ClientCreateAndDelete10VideoLicsFor10Times) {

}

TEST_F(LicsServerTests, 1000ClientCreateAndDelete10PictureLicsFor10Times) {

}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}