// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/message_loop.h"
#include "chrome/browser/renderer_host/renderer_security_policy.h"
#include "chrome/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/common/chrome_plugin_lib.h"
#include "chrome/common/render_messages.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"
#include "net/url_request/url_request_test_job.h"
#include "testing/gtest/include/gtest/gtest.h"

static int RequestIDForMessage(const IPC::Message& msg) {
  int request_id = -1;
  switch (msg.type()) {
    case ViewMsg_Resource_UploadProgress::ID:
    case ViewMsg_Resource_ReceivedResponse::ID:
    case ViewMsg_Resource_ReceivedRedirect::ID:
    case ViewMsg_Resource_DataReceived::ID:
    case ViewMsg_Resource_RequestComplete::ID:
      request_id = IPC::MessageIterator(msg).NextInt();
      break;
  }
  return request_id;
}

static ViewHostMsg_Resource_Request CreateResourceRequest(const char* method,
                                                          const GURL& url) {
  ViewHostMsg_Resource_Request request;
  request.method = std::string(method);
  request.url = url;
  request.policy_url = url;  // bypass third-party cookie blocking
  // init the rest to default values to prevent getting UMR.
  request.load_flags = 0;
  request.origin_pid = 0;
  request.resource_type = ResourceType::SUB_RESOURCE;
  request.mixed_content = false;
  return request;
}

// We may want to move this to a shared space if it is useful for something else
class ResourceIPCAccumulator {
 public:
  void AddMessage(const IPC::Message& msg) {
    messages_.push_back(msg);
  }

  // This groups the messages by their request ID. The groups will be in order
  // that the first message for each request ID was received, and the messages
  // within the groups will be in the order that they appeared.
  // Note that this clears messages_.
  typedef std::vector< std::vector<IPC::Message> > ClassifiedMessages;
  void GetClassifiedMessages(ClassifiedMessages* msgs);

  std::vector<IPC::Message> messages_;
};

// This is very inefficient as a result of repeatedly extracting the ID, use
// only for tests!
void ResourceIPCAccumulator::GetClassifiedMessages(ClassifiedMessages* msgs) {
  while (!messages_.empty()) {
    std::vector<IPC::Message> cur_requests;
    cur_requests.push_back(messages_[0]);
    int cur_id = RequestIDForMessage(messages_[0]);

    // find all other messages with this ID
    for (int i = 1; i < static_cast<int>(messages_.size()); i++) {
      int id = RequestIDForMessage(messages_[i]);
      if (id == cur_id) {
        cur_requests.push_back(messages_[i]);
        messages_.erase(messages_.begin() + i);
        i--;
      }
    }
    messages_.erase(messages_.begin());
    msgs->push_back(cur_requests);
  }
}

class ResourceDispatcherHostTest : public testing::Test,
                                   public ResourceDispatcherHost::Receiver {
 public:
  ResourceDispatcherHostTest() : host_(NULL) {
  }
  // ResourceDispatcherHost::Delegate implementation
  virtual bool Send(IPC::Message* msg) {
    accum_.AddMessage(*msg);
    delete msg;
    return true;
  }

 protected:
  // testing::Test
  virtual void SetUp() {
    RendererSecurityPolicy::GetInstance()->Add(0);
    URLRequest::RegisterProtocolFactory("test", &URLRequestTestJob::Factory);
    EnsureTestSchemeIsAllowed();
  }
  virtual void TearDown() {
    URLRequest::RegisterProtocolFactory("test", NULL);
    RendererSecurityPolicy::GetInstance()->Remove(0);

    // The plugin lib is automatically loaded during these test 
    // and we want a clean environment for other tests.
    ChromePluginLib::UnloadAllPlugins();

    // Flush the message loop to make Purify happy.
    message_loop_.RunAllPending();
  }

  void MakeTestRequest(int render_process_id,
                       int render_view_id,
                       int request_id,
                       const GURL& url);
  void MakeCancelRequest(int request_id);

  void EnsureTestSchemeIsAllowed() {
    static bool have_white_listed_test_scheme = false;

    if (!have_white_listed_test_scheme) {
      RendererSecurityPolicy::GetInstance()->RegisterWebSafeScheme("test");
      have_white_listed_test_scheme = true;
    }
  }

  MessageLoopForIO message_loop_;
  ResourceDispatcherHost host_;
  ResourceIPCAccumulator accum_;
};

// Spin up the message loop to kick off the request.
static void KickOffRequest() {
  MessageLoop::current()->RunAllPending();
}

void ResourceDispatcherHostTest::MakeTestRequest(int render_process_id,
                                                 int render_view_id,
                                                 int request_id,
                                                 const GURL& url) {
  ViewHostMsg_Resource_Request request = CreateResourceRequest("GET", url);

  host_.BeginRequest(this, GetCurrentProcess(), render_process_id,
                     render_view_id, request_id, request, NULL, NULL);
  KickOffRequest();
}

void ResourceDispatcherHostTest::MakeCancelRequest(int request_id) {
  host_.CancelRequest(0, request_id, false);
}

void CheckSuccessfulRequest(const std::vector<IPC::Message>& messages,
                            const std::string& reference_data) {
  // A successful request will have received 4 messages:
  //     ReceivedResponse    (indicates headers received)
  //     DataReceived        (data)
  //    XXX DataReceived        (0 bytes remaining from a read)
  //     RequestComplete     (request is done)
  //
  // This function verifies that we received 4 messages and that they
  // are appropriate.
  ASSERT_EQ(messages.size(), 3);

  // The first messages should be received response
  ASSERT_EQ(ViewMsg_Resource_ReceivedResponse::ID, messages[0].type());

  // followed by the data, currently we only do the data in one chunk, but
  // should probably test multiple chunks later
  ASSERT_EQ(ViewMsg_Resource_DataReceived::ID, messages[1].type());

  void* iter = NULL;
  int request_id;
  ASSERT_TRUE(ReadParam(&messages[1], &iter, &request_id));
  base::SharedMemoryHandle shm_handle;
  ASSERT_TRUE(ReadParam(&messages[1], &iter, &shm_handle));
  int data_len;
  ASSERT_TRUE(ReadParam(&messages[1], &iter, &data_len));

  ASSERT_EQ(reference_data.size(), data_len);
  base::SharedMemory shared_mem(shm_handle, true);  // read only
  shared_mem.Map(data_len);
  const char* data = static_cast<char*>(shared_mem.memory());
  ASSERT_EQ(0, memcmp(reference_data.c_str(), data, data_len));

  // followed by a 0-byte read
  //ASSERT_EQ(ViewMsg_Resource_DataReceived::ID, messages[2].type());

  // the last message should be all data received
  ASSERT_EQ(ViewMsg_Resource_RequestComplete::ID, messages[2].type());
}

// Tests whether many messages get dispatched properly.
TEST_F(ResourceDispatcherHostTest, TestMany) {
  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 0, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());

  // flush all the pending requests
  while (URLRequestTestJob::ProcessOnePendingMessage());

  // sorts out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are three requests, so we should have gotten them classified as such
  ASSERT_EQ(3, msgs.size());

  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[2], URLRequestTestJob::test_data_3());
}

// Tests whether messages get canceled properly. We issue three requests,
// cancel one of them, and make sure that each sent the proper notifications.
TEST_F(ResourceDispatcherHostTest, Cancel) {
  ResourceDispatcherHost host(NULL);

  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 0, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());
  MakeCancelRequest(2);

  // flush all the pending requests
  while (URLRequestTestJob::ProcessOnePendingMessage());

  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // there are three requests, so we should have gotten them classified as such
  ASSERT_EQ(3, msgs.size());

  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[2], URLRequestTestJob::test_data_3());

  // Check that request 2 got canceled before it finished reading, which gives
  // us 1 ReceivedResponse message.
  ASSERT_EQ(1, msgs[1].size());
  ASSERT_EQ(ViewMsg_Resource_ReceivedResponse::ID, msgs[1][0].type());

  // TODO(mbelshe):
  // Now that the async IO path is in place, the IO always completes on the
  // initial call; so the cancel doesn't arrive until after we finished.
  // This basically means the test doesn't work.
#if 0
  int request_id;
  URLRequestStatus status;

  // The message should be all data received with an error.
  ASSERT_EQ(ViewMsg_Resource_RequestComplete::ID, msgs[1][2].type());

  void* iter = NULL;
  ASSERT_TRUE(IPC::ReadParam(&msgs[1][2], &iter, &request_id));
  ASSERT_TRUE(IPC::ReadParam(&msgs[1][2], &iter, &status));

  EXPECT_EQ(URLRequestStatus::CANCELED, status.status());
#endif
}

// Tests CancelRequestsForProcess
TEST_F(ResourceDispatcherHostTest, TestProcessCancel) {
  // the host delegate acts as a second one so we can have some requests
  // pending and some canceled
  class TestReceiver : public ResourceDispatcherHost::Receiver {
   public:
    TestReceiver() : has_canceled_(false), received_after_canceled_(0) {
    }
    virtual bool Send(IPC::Message* msg) {
      // no messages should be received when the process has been canceled
      if (has_canceled_)
        received_after_canceled_ ++;
      delete msg;
      return true;
    }
    bool has_canceled_;
    int received_after_canceled_;
  };
  TestReceiver test_receiver;

  // request 1 goes to the test delegate
  ViewHostMsg_Resource_Request request =
      CreateResourceRequest("GET", URLRequestTestJob::test_url_1());

  host_.BeginRequest(&test_receiver, GetCurrentProcess(), 0, MSG_ROUTING_NONE,
                     1, request, NULL, NULL);
  KickOffRequest();

  // request 2 goes to us
  MakeTestRequest(0, 0, 2, URLRequestTestJob::test_url_2());

  // request 3 goes to the test delegate
  request.url = URLRequestTestJob::test_url_3();
  host_.BeginRequest(&test_receiver, GetCurrentProcess(), 0, MSG_ROUTING_NONE,
                     3, request, NULL, NULL);
  KickOffRequest();

  // TODO: mbelshe
  // Now that the async IO path is in place, the IO always completes on the
  // initial call; so the requests have already completed.  This basically
  // breaks the whole test.
  //EXPECT_EQ(3, host_.pending_requests());

  // process each request for one level so one callback is called
  for (int i = 0; i < 3; i++)
    EXPECT_TRUE(URLRequestTestJob::ProcessOnePendingMessage());

  // cancel the requests to the test process
  host_.CancelRequestsForProcess(0);
  test_receiver.has_canceled_ = true;

  // flush all the pending requests
  while (URLRequestTestJob::ProcessOnePendingMessage());

  EXPECT_EQ(0, host_.pending_requests());

  // the test delegate should not have gotten any messages after being canceled
  ASSERT_EQ(0, test_receiver.received_after_canceled_);

  // we should have gotten exactly one result
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1, msgs.size());
  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_2());
}

// Tests blocking and resuming requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingResumingRequests) {
  host_.BlockRequestsForRenderView(0, 1);
  host_.BlockRequestsForRenderView(0, 2);
  host_.BlockRequestsForRenderView(0, 3);

  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 1, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());
  MakeTestRequest(0, 1, 4, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, 5, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 3, 6, URLRequestTestJob::test_url_3());

  // Flush all the pending requests
  while (URLRequestTestJob::ProcessOnePendingMessage());

  // Sort out all the messages we saw by request
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // All requests but the 2 for the RVH 0 should have been blocked.
  ASSERT_EQ(2, msgs.size());

  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_3());

  // Resume requests for RVH 1 and flush pending requests.
  host_.ResumeBlockedRequestsForRenderView(0, 1);
  KickOffRequest();
  while (URLRequestTestJob::ProcessOnePendingMessage());

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2, msgs.size());
  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_1());

  // Test that new requests are not blocked for RVH 1.
  MakeTestRequest(0, 1, 7, URLRequestTestJob::test_url_1());
  while (URLRequestTestJob::ProcessOnePendingMessage());
  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(1, msgs.size());
  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());

  // Now resumes requests for all RVH (2 and 3).
  host_.ResumeBlockedRequestsForRenderView(0, 2);
  host_.ResumeBlockedRequestsForRenderView(0, 3);
  KickOffRequest();
  while (URLRequestTestJob::ProcessOnePendingMessage());

  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(2, msgs.size());
  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_2());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_3());
}

// Tests blocking and canceling requests.
TEST_F(ResourceDispatcherHostTest, TestBlockingCancelingRequests) {
  host_.BlockRequestsForRenderView(0, 1);

  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 1, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());
  MakeTestRequest(0, 1, 4, URLRequestTestJob::test_url_1());

  // Flush all the pending requests.
  while (URLRequestTestJob::ProcessOnePendingMessage());

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RVH 0 should have been processed.
  ASSERT_EQ(2, msgs.size());

  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_3());

  // Cancel requests for RVH 1.
  host_.CancelBlockedRequestsForRenderView(0, 1);
  KickOffRequest();
  while (URLRequestTestJob::ProcessOnePendingMessage());
  msgs.clear();
  accum_.GetClassifiedMessages(&msgs);
  ASSERT_EQ(0, msgs.size());
}

// Tests that blocked requests are canceled if their associated process dies.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsProcessDies) {
  host_.BlockRequestsForRenderView(1, 0);

  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(1, 0, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());
  MakeTestRequest(1, 0, 4, URLRequestTestJob::test_url_1());

  // Simulate process death.
  host_.CancelRequestsForProcess(1);

  // Flush all the pending requests.
  while (URLRequestTestJob::ProcessOnePendingMessage());

  // Sort out all the messages we saw by request.
  ResourceIPCAccumulator::ClassifiedMessages msgs;
  accum_.GetClassifiedMessages(&msgs);

  // The 2 requests for the RVH 0 should have been processed.
  ASSERT_EQ(2, msgs.size());

  CheckSuccessfulRequest(msgs[0], URLRequestTestJob::test_data_1());
  CheckSuccessfulRequest(msgs[1], URLRequestTestJob::test_data_3());

  EXPECT_TRUE(host_.blocked_requests_map_.empty());
}

// Tests that blocked requests don't leak when the ResourceDispatcherHost goes
// away.  Note that we rely on Purify for finding the leaks if any.
// If this test turns the Purify bot red, check the ResourceDispatcherHost
// destructor to make sure the blocked requests are deleted.
TEST_F(ResourceDispatcherHostTest, TestBlockedRequestsDontLeak) {
  host_.BlockRequestsForRenderView(0, 1);
  host_.BlockRequestsForRenderView(0, 2);
  host_.BlockRequestsForRenderView(1, 1);

  MakeTestRequest(0, 0, 1, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 1, 2, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 0, 3, URLRequestTestJob::test_url_3());
  MakeTestRequest(1, 1, 4, URLRequestTestJob::test_url_1());
  MakeTestRequest(0, 2, 5, URLRequestTestJob::test_url_2());
  MakeTestRequest(0, 2, 6, URLRequestTestJob::test_url_3());

  // Flush all the pending requests.
  while (URLRequestTestJob::ProcessOnePendingMessage());
}
