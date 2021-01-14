#include "server.h"

#include "gtest/gtest.h"

#define TEST_MAX_OA_LICS_NUM    (100000)
#define TEST_MAX_OD_LICS_NUM    (100000)
#define TEST_MAX_CLIENT_NUM   (1000)
#define TEST_MAX_CLIENT_LIMIT   (500)

#define TEST_10_LICS  (10)

class LicsServerTests : public testing::Test, public LicsServer {
    // virtual void SetUp() will be called before each test is run.  You
    // should define it if you need to initialize the variables.
    // Otherwise, this can be skipped.
    void SetUp() override {
      sleep(1);// make some time to get server ready for loading data.
    }

    // virtual void TearDown() will be called after each test is run.
    // You should define it if there is cleanup work to do.  Otherwise,
    // you don't have to provide it.
    //
    void TearDown() {
      Shutdown();
    }


    void pushAlgosUsedLicToCloud(const std::map<long, std::shared_ptr<AlgoLics>>& local) override {
        
    }

    void fetchAlgosTotalLicFromCloud(std::map<long, std::shared_ptr<AlgoLics>>& remote) override {

        std::shared_ptr<AlgoLics> oaLics = std::make_shared<AlgoLics>();
        oaLics->set_totallics(TEST_MAX_OA_LICS_NUM);
        oaLics->set_usedlics(0);

        std::shared_ptr<AlgoLics> odLics = std::make_shared<AlgoLics>();
        odLics->set_totallics(TEST_MAX_OD_LICS_NUM);
        odLics->set_usedlics(0);

        remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA] = oaLics;
        remote[UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD] = odLics;
    }

public:
  void Create10OdLic() {
    GetAuthAccessRequest authReq;
    GetAuthAccessResponse authResp;
    Status ret = getAuthAccess(&authReq,  &authResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(authResp.respcode(), ELICS_OK);

    CreateLicsRequest createReq;
    CreateLicsResponse createResp;
    createReq.set_token(authResp.token());
    createReq.set_clientexpectedlicsnum(TEST_10_LICS);
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::VIDEO);
    createReq.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
    ret = createLics(&createReq, &createResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(createResp.respcode(), ELICS_OK);
    EXPECT_EQ(createResp.clientgetactuallicsnum(), TEST_10_LICS);
  }

  void CreateAndDelete10OdLic() {
    GetAuthAccessRequest authReq;
    GetAuthAccessResponse authResp;
    Status ret = getAuthAccess(&authReq,  &authResp);
    EXPECT_TRUE(ret.ok());

    CreateLicsRequest createReq;
    CreateLicsResponse createResp;
    createReq.set_token(authResp.token());
    createReq.set_clientexpectedlicsnum(TEST_10_LICS);
    createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    createReq.mutable_algo()->set_type(TaskType::VIDEO);
    createReq.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
    ret = createLics(&createReq, &createResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(createResp.respcode(), ELICS_OK);
    EXPECT_EQ(createResp.clientgetactuallicsnum(), TEST_10_LICS);

    DeleteLicsRequest deleteReq;
    DeleteLicsResponse deleteResp;
    deleteReq.set_token(authResp.token());
    deleteReq.set_licsnum(TEST_10_LICS);
    deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
    deleteReq.mutable_algo()->set_type(TaskType::VIDEO);
    deleteReq.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
    ret =  deleteLics(&deleteReq, &deleteResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(deleteResp.respcode(), ELICS_OK);
  }

};

// interface tests
TEST_F(LicsServerTests, AuthShouldOk) {
  GetAuthAccessRequest request;
  GetAuthAccessResponse response;

  Status ret = getAuthAccess(&request,  &response);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(response.respcode(), ELICS_OK);
  EXPECT_GT(response.token(), 0);
}

TEST_F(LicsServerTests, CreateOdLicsShouldReturn10) {

  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  CreateLicsRequest req;
  CreateLicsResponse resp;
  req.set_token(authResp.token());
  req.set_clientexpectedlicsnum(TEST_10_LICS);
  req.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
  req.mutable_algo()->set_type(TaskType::VIDEO);
  req.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
  ret = createLics(&req, &resp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(resp.respcode(), ELICS_OK);
  EXPECT_EQ(resp.clientgetactuallicsnum(), TEST_10_LICS);

  int total, used;
  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, TEST_10_LICS);
  
}

TEST_F(LicsServerTests, 1ClientFetchOaLics) {
  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  KeepAliveRequest req;
  KeepAliveResponse resp;
  req.set_token(authResp.token());

  // upload picture lics to server
  for(int idx = 0; idx < 1; ++idx) { 
      AlgoLics* lics = req.add_lics();
      lics->set_requestid(0);
      lics->set_totallics(0);
      lics->set_usedlics(0);
      lics->set_maxlimit(TEST_MAX_CLIENT_LIMIT);

      lics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
      lics->mutable_algo()->set_type(TaskType::PICTURE);
      lics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA);
  }
  ret = keepAlive(&req, &resp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(resp.respcode(), ELICS_OK);

  for (int idx = 0; idx < resp.lics_size(); ++idx ) {
      int algoID = resp.lics(idx).algo().algorithmid();
      int total = resp.lics(idx).totallics();
      EXPECT_EQ(total, TEST_MAX_CLIENT_LIMIT); 
  }
}

TEST_F(LicsServerTests, DeleteOdLicsShouldReturnOk) {
  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  CreateLicsRequest createReq;
  CreateLicsResponse createResp;
  createReq.set_token(authResp.token());
  createReq.set_clientexpectedlicsnum(TEST_10_LICS);
  createReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
  createReq.mutable_algo()->set_type(TaskType::VIDEO);
  createReq.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
  ret = createLics(&createReq, &createResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(createResp.respcode(), ELICS_OK);
  EXPECT_EQ(createResp.clientgetactuallicsnum(), TEST_10_LICS);

  DeleteLicsRequest deleteReq;
  DeleteLicsResponse deleteResp;
  deleteReq.set_token(authResp.token());
  deleteReq.set_licsnum(TEST_10_LICS);
  deleteReq.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
  deleteReq.mutable_algo()->set_type(TaskType::VIDEO);
  deleteReq.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
  ret =  deleteLics(&deleteReq, &deleteResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(deleteResp.respcode(), ELICS_OK);
  
  int total, used;
  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, 0);

}

TEST_F(LicsServerTests,KeepAlive) {
  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  KeepAliveRequest req;
  KeepAliveResponse resp;
  req.set_token(authResp.token());

  // upload picture lics to server
  for(int idx = 0; idx < 1; ++idx) { 
      AlgoLics* lics = req.add_lics();
      lics->set_requestid(0);
      lics->set_totallics(0);
      lics->set_usedlics(0);
      lics->set_maxlimit(TEST_MAX_CLIENT_LIMIT);

      lics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
      lics->mutable_algo()->set_type(TaskType::PICTURE);
      lics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA);
  }
  ret = keepAlive(&req, &resp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(resp.respcode(), ELICS_OK);
}

// performance tests
TEST_F(LicsServerTests, ShouldHave1000Clients) {
  std::thread t[TEST_MAX_CLIENT_NUM];

  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
      t[tidx] =  std::thread(&LicsServerTests::Create10OdLic, this);
  }  
  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
      t[tidx].join();
  }

  EXPECT_EQ(totalClientNum(),TEST_MAX_CLIENT_NUM);
}

TEST_F(LicsServerTests, 1000ClientCreate10VideoLics) {

  std::thread t[TEST_MAX_CLIENT_NUM];

  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
      t[tidx] =  std::thread(&LicsServerTests::Create10OdLic, this);
  }  
  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
      t[tidx].join();
  }

  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  int total, used;
  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, TEST_MAX_CLIENT_NUM * TEST_10_LICS);
  EXPECT_EQ(totalClientNum(),TEST_MAX_CLIENT_NUM + 1);

}

TEST_F(LicsServerTests, 1000ClientCreateAndDelete10VideoLicsFor10Times) {

  int loopCnt = 10;
  for (int cnt = 0; cnt < loopCnt; ++cnt) {
      std::thread t[TEST_MAX_CLIENT_NUM];

      for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
          t[tidx] =  std::thread(&LicsServerTests::CreateAndDelete10OdLic, this);
      }  
      for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM; ++tidx) {
          t[tidx].join();
      }
  }

  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  int total, used;
  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, 0);

}

TEST_F(LicsServerTests, 1000ClientFetchOaLics) {
  for (int clientIdx = 0; clientIdx < TEST_MAX_CLIENT_NUM; clientIdx++) {
    GetAuthAccessRequest authReq;
    GetAuthAccessResponse authResp;
    Status ret = getAuthAccess(&authReq,  &authResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(authResp.respcode(), ELICS_OK);

    KeepAliveRequest req;
    KeepAliveResponse resp;
    req.set_token(authResp.token());

    // upload picture lics to server
    for(int idx = 0; idx < 1; ++idx) { 
        AlgoLics* lics = req.add_lics();
        lics->set_requestid(0);
        lics->set_totallics(0);
        lics->set_usedlics(0);
        lics->set_maxlimit(TEST_MAX_CLIENT_LIMIT);

        lics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
        lics->mutable_algo()->set_type(TaskType::PICTURE);
        lics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA);
    }
    ret = keepAlive(&req, &resp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(resp.respcode(), ELICS_OK);

    for (int idx = 0; idx < resp.lics_size(); ++idx ) {
        int algoID = resp.lics(idx).algo().algorithmid();
        int licsNum = resp.lics(idx).totallics();

        int benchMark = TEST_MAX_OA_LICS_NUM/(clientIdx+1) > TEST_MAX_CLIENT_LIMIT ? TEST_MAX_CLIENT_LIMIT :  TEST_MAX_OA_LICS_NUM/(clientIdx+1);
        EXPECT_EQ(licsNum, benchMark); 
    }
  }
}

TEST_F(LicsServerTests, 500ClientCreateOdAnd500ClientFetchOaLics) {

// 500 clients create od lics
  std::thread t[TEST_MAX_CLIENT_NUM / 2];

  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM / 2; ++tidx) {
      t[tidx] =  std::thread(&LicsServerTests::Create10OdLic, this);
  }  
  for (int tidx = 0; tidx < TEST_MAX_CLIENT_NUM / 2; ++tidx) {
      t[tidx].join();
  }

  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  int total, used;
  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, TEST_MAX_CLIENT_NUM / 2 * TEST_10_LICS);
  EXPECT_EQ(totalClientNum(),TEST_MAX_CLIENT_NUM / 2 + 1);


  // 500 clients fetch oa lics
  for (int clientIdx = 0; clientIdx < TEST_MAX_CLIENT_NUM / 2; clientIdx++) {
    GetAuthAccessRequest authReq;
    GetAuthAccessResponse authResp;
    Status ret = getAuthAccess(&authReq,  &authResp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(authResp.respcode(), ELICS_OK);

    KeepAliveRequest req;
    KeepAliveResponse resp;
    req.set_token(authResp.token());

    // upload picture lics to server
    for(int idx = 0; idx < 1; ++idx) { 
        AlgoLics* lics = req.add_lics();
        lics->set_requestid(0);
        lics->set_totallics(0);
        lics->set_usedlics(0);
        lics->set_maxlimit(TEST_MAX_CLIENT_LIMIT);

        lics->mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
        lics->mutable_algo()->set_type(TaskType::PICTURE);
        lics->mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OA);
    }
    ret = keepAlive(&req, &resp);
    EXPECT_TRUE(ret.ok());
    EXPECT_EQ(resp.respcode(), ELICS_OK);

    for (int idx = 0; idx < resp.lics_size(); ++idx ) {
        int algoID = resp.lics(idx).algo().algorithmid();
        int licsNum = resp.lics(idx).totallics();

        int benchMark = TEST_MAX_OA_LICS_NUM/(clientIdx+1) > TEST_MAX_CLIENT_LIMIT ? TEST_MAX_CLIENT_LIMIT :  TEST_MAX_OA_LICS_NUM/(clientIdx+1);
        EXPECT_EQ(licsNum, benchMark); 
    }
  }
}

TEST_F(LicsServerTests, MakeClientTimeout) {
  GetAuthAccessRequest authReq;
  GetAuthAccessResponse authResp;
  Status ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  CreateLicsRequest req;
  CreateLicsResponse resp;
  req.set_token(authResp.token());
  req.set_clientexpectedlicsnum(TEST_10_LICS);
  req.mutable_algo()->set_vendor(Vendor::UNISINSIGHT);
  req.mutable_algo()->set_type(TaskType::VIDEO);
  req.mutable_algo()->set_algorithmid(UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD);
  ret = createLics(&req, &resp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(resp.respcode(), ELICS_OK);
  EXPECT_EQ(resp.clientgetactuallicsnum(), TEST_10_LICS);

  // 3*30s heatbeat-stop detect to make server clear the client.
  std::cout <<"please wait 90s, testing....." << std::endl;
  for (int idx = 0; idx < 3; ++idx) {
    EXPECT_EQ(totalClientNum(),1);
    sleep(40); 
  }

  EXPECT_EQ(totalClientNum(),0);

  int total, used;
  ret = getAuthAccess(&authReq,  &authResp);
  EXPECT_TRUE(ret.ok());
  EXPECT_EQ(authResp.respcode(), ELICS_OK);

  licsQuery(authResp.token(), UNIS_FACE_PERSON_VEHICLE_NONVEHICLE_OD, total, used);
  EXPECT_EQ(total, TEST_MAX_OD_LICS_NUM);
  EXPECT_EQ(used, 0);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}