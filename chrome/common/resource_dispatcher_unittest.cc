// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/process.h"
#include "base/ref_counted.h"
#include "chrome/common/filter_policy.h"
#include "chrome/common/resource_dispatcher.h"

#include "testing/gtest/include/gtest/gtest.h"

using webkit_glue::ResourceLoaderBridge;

static const char test_page_url[] = "http://www.google.com/";
static const char test_page_headers[] =
  "HTTP/1.1 200 OK\nContent-Type:text/html\n\n";
static const char test_page_mime_type[] = "text/html";
static const char test_page_charset[] = "";
static const char test_page_contents[] =
  "<html><head><title>Google</title></head><body><h1>Google</h1></body></html>";
static const int test_page_contents_len = arraysize(test_page_contents) - 1;

// Listens for request response data and stores it so that it can be compared
// to the reference data.
class TestRequestCallback : public ResourceLoaderBridge::Peer {
 public:
  TestRequestCallback() : complete_(false) {
  }

  virtual void OnReceivedRedirect(const GURL& new_url) {
  }

  virtual void OnReceivedResponse(
      const ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered) {
  }

  virtual void OnReceivedData(const char* data, int len) {
    EXPECT_FALSE(complete_);
    data_.append(data, len);
  }

  virtual void OnCompletedRequest(const URLRequestStatus& status) {
    EXPECT_FALSE(complete_);
    complete_ = true;
  }

  virtual std::string GetURLForDebugging() {
    return std::string();
  }

  const std::string& data() const {
    return data_;
  }
  const bool complete() const {
    return complete_;
  }

 private:
  bool complete_;
  std::string data_;
};


// Sets up the message sender override for the unit test
class ResourceDispatcherTest : public testing::Test,
                               public IPC::Message::Sender {
 public:
  // Emulates IPC send operations (IPC::Message::Sender) by adding
  // pending messages to the queue.
  virtual bool Send(IPC::Message* msg) {
    message_queue_.push_back(IPC::Message(*msg));
    delete msg;
    return true;
  }

  // Emulates the browser process and processes the pending IPC messages,
  // returning the hardcoded file contents.
  void ProcessMessages() {
    while (!message_queue_.empty()) {
      void* iter = NULL;

      int request_id;
      ASSERT_TRUE(IPC::ReadParam(&message_queue_[0], &iter, &request_id));

      ViewHostMsg_Resource_Request request;
      ASSERT_TRUE(IPC::ReadParam(&message_queue_[0], &iter, &request));

      // check values
      EXPECT_EQ(test_page_url, request.url.spec());

      // received response message
      ViewMsg_Resource_ResponseHead response;
      std::string raw_headers(test_page_headers);
      std::replace(raw_headers.begin(), raw_headers.end(), '\n', '\0');
      response.headers = new net::HttpResponseHeaders(raw_headers);
      response.mime_type = test_page_mime_type;
      response.charset = test_page_charset;
      response.filter_policy = FilterPolicy::DONT_FILTER;
      dispatcher_->OnReceivedResponse(request_id, response);

      // received data message with the test contents
      base::SharedMemory shared_mem;
      EXPECT_TRUE(shared_mem.Create(std::wstring(),
          false, false, test_page_contents_len));
      EXPECT_TRUE(shared_mem.Map(test_page_contents_len));
      char* put_data_here = static_cast<char*>(shared_mem.memory());
      memcpy(put_data_here, test_page_contents, test_page_contents_len);
      base::SharedMemoryHandle dup_handle;
      EXPECT_TRUE(shared_mem.GiveToProcess(
          base::Process::Current().handle(), &dup_handle));
      dispatcher_->OnReceivedData(request_id, dup_handle, test_page_contents_len);

      message_queue_.erase(message_queue_.begin());

      // read the ack message.
      iter = NULL;
      int request_ack = -1;
      ASSERT_TRUE(IPC::ReadParam(&message_queue_[0], &iter, &request_ack));

      ASSERT_EQ(request_ack, request_id);

      message_queue_.erase(message_queue_.begin());
    }
  }

 protected:
  static ResourceDispatcher* GetResourceDispatcher(WebFrame* unused) {
    return dispatcher_;
  }

  // testing::Test
  virtual void SetUp() {
    dispatcher_ = new ResourceDispatcher(this);
  }
  virtual void TearDown() {
    dispatcher_ = NULL;
  }

  std::vector<IPC::Message> message_queue_;
  static scoped_refptr<ResourceDispatcher> dispatcher_;
};

/*static*/
scoped_refptr<ResourceDispatcher> ResourceDispatcherTest::dispatcher_;

// Does a simple request and tests that the correct data is received.
TEST_F(ResourceDispatcherTest, RoundTrip) {
  TestRequestCallback callback;
  ResourceLoaderBridge* bridge =
    dispatcher_->CreateBridge("GET", GURL(test_page_url), GURL(test_page_url),
                              GURL(), std::string(), 0, 0,
                              ResourceType::SUB_RESOURCE, false, 0);

  bridge->Start(&callback);

  ProcessMessages();

  // FIXME(brettw) when the request complete messages are actually handledo
  // and dispatched, uncomment this.
  //EXPECT_TRUE(callback.complete());
  //EXPECT_STREQ(test_page_contents, callback.data().c_str());

  delete bridge;
}

// Tests that the request IDs are straight when there are multiple requests.
TEST_F(ResourceDispatcherTest, MultipleRequests) {
  // FIXME
}

// Tests that the cancel method prevents other messages from being received
TEST_F(ResourceDispatcherTest, Cancel) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, Cookies) {
  // FIXME
}

TEST_F(ResourceDispatcherTest, SerializedPostData) {
  // FIXME
}

