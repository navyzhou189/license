#include "server.h"

#include "gtest/gtest.h"

class LicsServerVideo : public testing::Test, public LicsServer {
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


    void updateCloudAlgosUsedLic(const std::map<long, std::shared_ptr<AlgoLics>>& licenseQ) {

    }

    void fetchCloudAlgosTotalLic(std::map<long, std::shared_ptr<AlgoLics>>& remoteAlgosTotalLic) {

    }


};

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}