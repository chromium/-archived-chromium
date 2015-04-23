// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/client_socket_pool_base.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/net_errors.h"
#include "net/base/test_completion_callback.h"
#include "net/socket/client_socket.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/client_socket_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace {

const int kMaxSocketsPerGroup = 2;

// Note that the first and the last are the same, the first should be handled
// before the last, since it was inserted first.
const int kPriorities[] = { 1, 3, 4, 2, 1 };

// This is the number of extra requests beyond the first few that use up all
// available sockets in the socket group.
const int kNumPendingRequests = arraysize(kPriorities);

const int kNumRequests = kMaxSocketsPerGroup + kNumPendingRequests;

const int kDefaultPriority = 5;

class MockClientSocket : public ClientSocket {
 public:
  MockClientSocket() : connected_(false) {}

  // Socket methods:
  virtual int Read(
      IOBuffer* /* buf */, int /* len */, CompletionCallback* /* callback */) {
    return ERR_UNEXPECTED;
  }

  virtual int Write(
      IOBuffer* /* buf */, int /* len */, CompletionCallback* /* callback */) {
    return ERR_UNEXPECTED;
  }

  // ClientSocket methods:

  virtual int Connect(CompletionCallback* callback) {
    connected_ = true;
    return OK;
  }

  virtual void Disconnect() { connected_ = false; }
  virtual bool IsConnected() const { return connected_; }
  virtual bool IsConnectedAndIdle() const { return connected_; }

#if defined(OS_LINUX)
  virtual int GetPeerName(struct sockaddr* /* name */,
                          socklen_t* /* namelen */) {
    return 0;
  }
#endif

 private:
  bool connected_;

  DISALLOW_COPY_AND_ASSIGN(MockClientSocket);
};

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  MockClientSocketFactory() : allocation_count_(0) {}

  virtual ClientSocket* CreateTCPClientSocket(const AddressList& addresses) {
    allocation_count_++;
    return NULL;
  }

  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config) {
    NOTIMPLEMENTED();
    return NULL;
  }

  int allocation_count() const { return allocation_count_; }

 private:
  int allocation_count_;
};

class TestSocketRequest : public CallbackRunner< Tuple1<int> > {
 public:
  TestSocketRequest(
      ClientSocketPool* pool,
      std::vector<TestSocketRequest*>* request_order)
      : handle(pool), request_order_(request_order) {}

  ClientSocketHandle handle;

  int WaitForResult() {
    return callback_.WaitForResult();
  }

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    completion_count++;
    request_order_->push_back(this);
  }

  static int completion_count;

 private:
  std::vector<TestSocketRequest*>* request_order_;
  TestCompletionCallback callback_;
};

int TestSocketRequest::completion_count = 0;

class TestConnectJob : public ConnectJob {
 public:
  enum JobType {
    kMockJob,
    kMockFailingJob,
    kMockPendingJob,
    kMockPendingFailingJob,
  };

  TestConnectJob(JobType job_type,
                 const std::string& group_name,
                 const ClientSocketPoolBase::Request& request,
                 ConnectJob::Delegate* delegate,
                 ClientSocketFactory* client_socket_factory)
      : ConnectJob(group_name, request.handle, delegate),
        job_type_(job_type),
        client_socket_factory_(client_socket_factory),
        method_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  // ConnectJob methods:

  virtual int Connect() {
    AddressList ignored;
    client_socket_factory_->CreateTCPClientSocket(ignored);
    switch (job_type_) {
      case kMockJob:
        return DoConnect(true /* successful */, false /* sync */);
      case kMockFailingJob:
        return DoConnect(false /* error */, false /* sync */);
      case kMockPendingJob:
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
               &TestConnectJob::DoConnect,
               true /* successful */,
               true /* async */));
        return ERR_IO_PENDING;
      case kMockPendingFailingJob:
        MessageLoop::current()->PostTask(
            FROM_HERE,
            method_factory_.NewRunnableMethod(
               &TestConnectJob::DoConnect,
               false /* error */,
               true  /* async */));
        return ERR_IO_PENDING;
      default:
        NOTREACHED();
        return ERR_FAILED;
    }
  }

 private:
  int DoConnect(bool succeed, bool was_async) {
    int result = ERR_CONNECTION_FAILED;
    if (succeed) {
      result = OK;
      set_socket(new MockClientSocket());
      socket()->Connect(NULL);
    }

    if (was_async)
      delegate()->OnConnectJobComplete(result, this);
    return result;
  }

  const JobType job_type_;
  ClientSocketFactory* const client_socket_factory_;
  ScopedRunnableMethodFactory<TestConnectJob> method_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectJob);
};

class TestConnectJobFactory : public ClientSocketPoolBase::ConnectJobFactory {
 public:
  explicit TestConnectJobFactory(ClientSocketFactory* client_socket_factory)
      : job_type_(TestConnectJob::kMockJob),
        client_socket_factory_(client_socket_factory) {}

  virtual ~TestConnectJobFactory() {}

  void set_job_type(TestConnectJob::JobType job_type) { job_type_ = job_type; }

  // ConnectJobFactory methods:

  virtual ConnectJob* NewConnectJob(
      const std::string& group_name,
      const ClientSocketPoolBase::Request& request,
      ConnectJob::Delegate* delegate) const {
    return new TestConnectJob(job_type_,
                              group_name,
                              request,
                              delegate,
                              client_socket_factory_);
  }

 private:
  TestConnectJob::JobType job_type_;
  ClientSocketFactory* const client_socket_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestConnectJobFactory);
};

class TestClientSocketPool : public ClientSocketPool {
 public:
  TestClientSocketPool(
      int max_sockets_per_group,
      ClientSocketPoolBase::ConnectJobFactory* connect_job_factory)
      : base_(new ClientSocketPoolBase(
          kMaxSocketsPerGroup, connect_job_factory)) {}

  virtual int RequestSocket(
      const std::string& group_name,
      const HostResolver::RequestInfo& resolve_info,
      int priority,
      ClientSocketHandle* handle,
      CompletionCallback* callback) {
    return base_->RequestSocket(
        group_name, resolve_info, priority, handle, callback);
  }

  virtual void CancelRequest(
      const std::string& group_name,
      const ClientSocketHandle* handle) {
    base_->CancelRequest(group_name, handle);
  }

  virtual void ReleaseSocket(
      const std::string& group_name,
      ClientSocket* socket) {
    base_->ReleaseSocket(group_name, socket);
  }

  virtual void CloseIdleSockets() {
    base_->CloseIdleSockets();
  }

  virtual int IdleSocketCount() const { return base_->idle_socket_count(); }

  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    return base_->IdleSocketCountInGroup(group_name);
  }

  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    return base_->GetLoadState(group_name, handle);
  }

 private:
  const scoped_refptr<ClientSocketPoolBase> base_;

  DISALLOW_COPY_AND_ASSIGN(TestClientSocketPool);
};

class ClientSocketPoolBaseTest : public testing::Test {
 protected:
  ClientSocketPoolBaseTest()
      : ignored_request_info_("ignored", 80),
        connect_job_factory_(
          new TestConnectJobFactory(&client_socket_factory_)),
        pool_(new TestClientSocketPool(kMaxSocketsPerGroup,
                                       connect_job_factory_)) {}

  virtual void SetUp() {
    TestSocketRequest::completion_count = 0;
  }

  virtual void TearDown() {
    // The tests often call Reset() on handles at the end which may post
    // DoReleaseSocket() tasks.
    MessageLoop::current()->RunAllPending();
  }

  void CreateConnections(scoped_ptr<TestSocketRequest>* reqs, size_t count) {
    for (size_t i = 0; i < count; ++i)
      reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));

    // Create connections or queue up requests.
    for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
      int rv = reqs[i]->handle.Init("a", ignored_request_info_,
                                    kDefaultPriority, reqs[i].get());
      EXPECT_EQ(OK, rv);
      request_order_.push_back(reqs[i].get());
    }

    // The rest are pending since we've used all active sockets.
    for (int i = 0; i < kNumPendingRequests; ++i) {
      int rv = reqs[kMaxSocketsPerGroup + i]->handle.Init(
          "a", ignored_request_info_, kPriorities[i],
          reqs[kMaxSocketsPerGroup + i].get());
      EXPECT_EQ(ERR_IO_PENDING, rv);
    }
  }

  enum KeepAlive {
    KEEP_ALIVE,
    NO_KEEP_ALIVE,
  };

  void ReleaseAllConnections(scoped_ptr<TestSocketRequest>* reqs,
                             size_t count, KeepAlive keep_alive) {
    bool released_one;
    do {
      released_one = false;
      for (size_t i = 0; i < count; ++i) {
        if (reqs[i]->handle.is_initialized()) {
          if (keep_alive == NO_KEEP_ALIVE)
            reqs[i]->handle.socket()->Disconnect();
          reqs[i]->handle.Reset();
          MessageLoop::current()->RunAllPending();
          released_one = true;
        }
      }
    } while (released_one);
  }

  HostResolver::RequestInfo ignored_request_info_;
  MockClientSocketFactory client_socket_factory_;
  TestConnectJobFactory* const connect_job_factory_;
  scoped_refptr<ClientSocketPool> pool_;
  std::vector<TestSocketRequest*> request_order_;
};

TEST_F(ClientSocketPoolBaseTest, Basic) {
  TestCompletionCallback callback;
  ClientSocketHandle handle(pool_.get());
  EXPECT_EQ(OK, handle.Init("a", ignored_request_info_, kDefaultPriority,
                            &callback));
  EXPECT_TRUE(handle.is_initialized());
  EXPECT_TRUE(handle.socket());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, InitConnectionFailure) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockFailingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_CONNECTION_FAILED,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  CreateConnections(reqs, arraysize(reqs));
  ReleaseAllConnections(reqs, arraysize(reqs), KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);

  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    EXPECT_EQ(request_order_[i], reqs[i].get()) <<
        "Request " << i << " was not in order.";
  }

  for (int i = 0; i < kNumPendingRequests - 1; ++i) {
    int index_in_queue = (kNumPendingRequests - 1) - kPriorities[i];
    EXPECT_EQ(request_order_[kMaxSocketsPerGroup + index_in_queue],
              reqs[kMaxSocketsPerGroup + i].get()) <<
        "Request " << kMaxSocketsPerGroup + i << " was not in order.";
  }

  EXPECT_EQ(request_order_[arraysize(reqs) - 1],
            reqs[arraysize(reqs) - 1].get()) <<
      "The last request with priority 1 should not have been inserted "
      "earlier into the queue.";
}

TEST_F(ClientSocketPoolBaseTest, PendingRequests_NoKeepAlive) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  CreateConnections(reqs, arraysize(reqs));
  ReleaseAllConnections(reqs, arraysize(reqs), NO_KEEP_ALIVE);

  for (int i = kMaxSocketsPerGroup; i < kNumRequests; ++i)
    EXPECT_EQ(OK, reqs[i]->WaitForResult());

  EXPECT_EQ(kNumRequests, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);
}

// This test will start up a RequestSocket() and then immediately Cancel() it.
// The pending connect job will be cancelled and should not call back into
// ClientSocketPoolBase.
TEST_F(ClientSocketPoolBaseTest, CancelRequestClearGroup) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  req.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, TwoRequestsCancelOne) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  TestSocketRequest req(pool_.get(), &request_order_);
  TestSocketRequest req2(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            req.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req));
  EXPECT_EQ(ERR_IO_PENDING,
            req2.handle.Init("a", ignored_request_info_,
                             kDefaultPriority, &req2));

  req.handle.Reset();

  EXPECT_EQ(OK, req2.WaitForResult());
  req2.handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, ConnectCancelConnect) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  TestCompletionCallback callback;
  TestSocketRequest req(pool_.get(), &request_order_);

  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback));

  handle.Reset();

  TestCompletionCallback callback2;
  EXPECT_EQ(ERR_IO_PENDING,
            handle.Init("a", ignored_request_info_,
                        kDefaultPriority, &callback2));

  EXPECT_EQ(OK, callback2.WaitForResult());
  EXPECT_FALSE(callback.have_result());

  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, CancelRequest) {
  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  CreateConnections(reqs, arraysize(reqs));

  // Cancel a request.
  size_t index_to_cancel = kMaxSocketsPerGroup + 2;
  EXPECT_TRUE(!reqs[index_to_cancel]->handle.is_initialized());
  reqs[index_to_cancel]->handle.Reset();

  ReleaseAllConnections(reqs, arraysize(reqs), KEEP_ALIVE);

  EXPECT_EQ(kMaxSocketsPerGroup, client_socket_factory_.allocation_count());
  EXPECT_EQ(kNumPendingRequests - 1, TestSocketRequest::completion_count);

  for (int i = 0; i < kMaxSocketsPerGroup; ++i) {
    EXPECT_EQ(request_order_[i], reqs[i].get()) <<
        "Request " << i << " was not in order.";
  }

  for (int i = 0; i < kNumPendingRequests - 1; ++i) {
    if (i == 2) continue;
    int index_in_queue = (kNumPendingRequests - 1) - kPriorities[i];
    if (kPriorities[i] < kPriorities[index_to_cancel - kMaxSocketsPerGroup])
      index_in_queue--;
    EXPECT_EQ(request_order_[kMaxSocketsPerGroup + index_in_queue],
              reqs[kMaxSocketsPerGroup + i].get()) <<
        "Request " << kMaxSocketsPerGroup + i << " was not in order.";
  }

  EXPECT_EQ(request_order_[arraysize(reqs) - 2],
            reqs[arraysize(reqs) - 1].get()) <<
      "The last request with priority 1 should not have been inserted "
      "earlier into the queue.";
}

class RequestSocketCallback : public CallbackRunner< Tuple1<int> > {
 public:
  RequestSocketCallback(ClientSocketHandle* handle,
                        TestConnectJobFactory* test_connect_job_factory,
                        TestConnectJob::JobType next_job_type)
      : handle_(handle),
        within_callback_(false),
        test_connect_job_factory_(test_connect_job_factory),
        next_job_type_(next_job_type) {}

  virtual void RunWithParams(const Tuple1<int>& params) {
    callback_.RunWithParams(params);
    ASSERT_EQ(OK, params.a);

    if (!within_callback_) {
      test_connect_job_factory_->set_job_type(next_job_type_);
      handle_->Reset();
      within_callback_ = true;
      int rv = handle_->Init(
          "a", HostResolver::RequestInfo("www.google.com", 80),
          kDefaultPriority, this);
      switch (next_job_type_) {
        case TestConnectJob::kMockJob:
          EXPECT_EQ(OK, rv);
          break;
        case TestConnectJob::kMockPendingJob:
          EXPECT_EQ(ERR_IO_PENDING, rv);
          break;
        default:
          FAIL() << "Unexpected job type: " << next_job_type_;
          break;
      }
    }
  }

  int WaitForResult() {
    return callback_.WaitForResult();
  }

 private:
  ClientSocketHandle* const handle_;
  bool within_callback_;
  TestConnectJobFactory* const test_connect_job_factory_;
  TestConnectJob::JobType next_job_type_;
  TestCompletionCallback callback_;
};

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobTwice) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockPendingJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

TEST_F(ClientSocketPoolBaseTest, RequestPendingJobThenSynchronous) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);
  ClientSocketHandle handle(pool_.get());
  RequestSocketCallback callback(
      &handle, connect_job_factory_, TestConnectJob::kMockJob);
  int rv = handle.Init(
      "a", ignored_request_info_, kDefaultPriority, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(OK, callback.WaitForResult());
  handle.Reset();
}

// Make sure that pending requests get serviced after active requests get
// cancelled.
TEST_F(ClientSocketPoolBaseTest, CancelActiveRequestWithPendingRequests) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingJob);

  scoped_ptr<TestSocketRequest> reqs[kNumRequests];

  // Queue up all the requests
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", ignored_request_info_,
                                  kDefaultPriority, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  // Now, kMaxSocketsPerGroup requests should be active.  Let's cancel them.
  for (int i = 0; i < kMaxSocketsPerGroup; ++i)
    reqs[i]->handle.Reset();

  // Let's wait for the rest to complete now.
  for (size_t i = kMaxSocketsPerGroup; i < arraysize(reqs); ++i) {
    EXPECT_EQ(OK, reqs[i]->WaitForResult());
    reqs[i]->handle.Reset();
  }

  EXPECT_EQ(kNumPendingRequests, TestSocketRequest::completion_count);
}

// Make sure that pending requests get serviced after active requests fail.
TEST_F(ClientSocketPoolBaseTest, FailingActiveRequestWithPendingRequests) {
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  scoped_ptr<TestSocketRequest> reqs[kMaxSocketsPerGroup * 2 + 1];

  // Queue up all the requests
  for (size_t i = 0; i < arraysize(reqs); ++i) {
    reqs[i].reset(new TestSocketRequest(pool_.get(), &request_order_));
    int rv = reqs[i]->handle.Init("a", ignored_request_info_,
                                  kDefaultPriority, reqs[i].get());
    EXPECT_EQ(ERR_IO_PENDING, rv);
  }

  for (size_t i = 0; i < arraysize(reqs); ++i)
    EXPECT_EQ(ERR_CONNECTION_FAILED, reqs[i]->WaitForResult());
}

// A pending asynchronous job completes, which will free up a socket slot.  The
// next job finishes synchronously.  The callback for the asynchronous job
// should be first though.
TEST_F(ClientSocketPoolBaseTest, PendingJobCompletionOrder) {
  // First two jobs are async.
  connect_job_factory_->set_job_type(TestConnectJob::kMockPendingFailingJob);

  // Start job 1 (async error).
  TestSocketRequest req1(pool_.get(), &request_order_);
  int rv = req1.handle.Init("a", ignored_request_info_,
                            kDefaultPriority, &req1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // Start job 2 (async error).
  TestSocketRequest req2(pool_.get(), &request_order_);
  rv = req2.handle.Init("a", ignored_request_info_, kDefaultPriority, &req2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  // The pending job is sync.
  connect_job_factory_->set_job_type(TestConnectJob::kMockJob);

  // Request 3 does not have a ConnectJob yet.  It's just pending.
  TestSocketRequest req3(pool_.get(), &request_order_);
  rv = req3.handle.Init("a", ignored_request_info_, kDefaultPriority, &req3);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  EXPECT_EQ(ERR_CONNECTION_FAILED, req1.WaitForResult());
  EXPECT_EQ(ERR_CONNECTION_FAILED, req2.WaitForResult());
  EXPECT_EQ(OK, req3.WaitForResult());

  ASSERT_EQ(3U, request_order_.size());

  // After job 1 finishes unsuccessfully, it will try to process the pending
  // requests queue, so it starts up job 3 for request 3.  This job
  // synchronously succeeds, so the request order is 1, 3, 2.
  EXPECT_EQ(&req1, request_order_[0]);
  EXPECT_EQ(&req2, request_order_[2]);
  EXPECT_EQ(&req3, request_order_[1]);
}

}  // namespace

}  // namespace net
