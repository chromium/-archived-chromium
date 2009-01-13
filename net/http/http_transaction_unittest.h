// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_
#define NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_

#include "net/http/http_transaction.h"

#include <algorithm>
#include <string>

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "base/string_util.h"
#include "net/base/net_errors.h"
#include "net/base/load_flags.h"
#include "net/base/test_completion_callback.h"
#include "net/disk_cache/disk_cache.h"
#include "net/http/http_cache.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_info.h"

//-----------------------------------------------------------------------------
// mock transaction data

// these flags may be combined to form the test_mode field
enum {
  TEST_MODE_NORMAL = 0,
  TEST_MODE_SYNC_NET_START = 1 << 0,
  TEST_MODE_SYNC_NET_READ  = 1 << 1,
  TEST_MODE_SYNC_CACHE_START = 1 << 2,
  TEST_MODE_SYNC_CACHE_READ  = 1 << 3,
};

typedef void (*MockTransactionHandler)(const net::HttpRequestInfo* request,
                                       std::string* response_status,
                                       std::string* response_headers,
                                       std::string* response_data);

struct MockTransaction {
  const char* url;
  const char* method;
  const char* request_headers;
  int load_flags;
  const char* status;
  const char* response_headers;
  const char* data;
  int test_mode;
  MockTransactionHandler handler;
  int cert_status;
};

extern const MockTransaction kSimpleGET_Transaction;
extern const MockTransaction kSimplePOST_Transaction;
extern const MockTransaction kTypicalGET_Transaction;
extern const MockTransaction kETagGET_Transaction;
extern const MockTransaction kRangeGET_Transaction;

// returns the mock transaction for the given URL
const MockTransaction* FindMockTransaction(const GURL& url);

// Add/Remove a mock transaction that can be accessed via FindMockTransaction.
// There can be only one MockTransaction associated with a given URL.
void AddMockTransaction(const MockTransaction* trans);
void RemoveMockTransaction(const MockTransaction* trans);

struct ScopedMockTransaction : MockTransaction {
  ScopedMockTransaction() {
    AddMockTransaction(this);
  }
  explicit ScopedMockTransaction(const MockTransaction& t)
      : MockTransaction(t) {
    AddMockTransaction(this);
  }
  ~ScopedMockTransaction() {
    RemoveMockTransaction(this);
  }
};

//-----------------------------------------------------------------------------
// mock http request

class MockHttpRequest : public net::HttpRequestInfo {
 public:
  explicit MockHttpRequest(const MockTransaction& t) {
    url = GURL(t.url);
    method = t.method;
    extra_headers = t.request_headers;
    load_flags = t.load_flags;
  }
};

//-----------------------------------------------------------------------------
// use this class to test completely consuming a transaction

class TestTransactionConsumer : public CallbackRunner< Tuple1<int> > {
 public:
  explicit TestTransactionConsumer(net::HttpTransactionFactory* factory)
      : state_(IDLE),
        trans_(factory->CreateTransaction()),
        error_(net::OK) {
    ++quit_counter_;
  }

  ~TestTransactionConsumer() {
  }

  void Start(const net::HttpRequestInfo* request) {
    state_ = STARTING;
    int result = trans_->Start(request, this);
    if (result != net::ERR_IO_PENDING)
      DidStart(result);
  }

  bool is_done() const { return state_ == DONE; }
  int error() const { return error_; }

  const net::HttpResponseInfo* response_info() const {
    return trans_->GetResponseInfo();
  }
  const std::string& content() const { return content_; }

 private:
  // Callback implementation:
  virtual void RunWithParams(const Tuple1<int>& params) {
    int result = params.a;
    switch (state_) {
      case STARTING:
        DidStart(result);
        break;
      case READING:
        DidRead(result);
        break;
      default:
        NOTREACHED();
    }
  }

  void DidStart(int result) {
    if (result != net::OK) {
      DidFinish(result);
    } else {
      Read();
    }
  }

  void DidRead(int result) {
    if (result <= 0) {
      DidFinish(result);
    } else {
      content_.append(read_buf_, result);
      Read();
    }
  }

  void DidFinish(int result) {
    state_ = DONE;
    error_ = result;
    if (--quit_counter_ == 0)
      MessageLoop::current()->Quit();
  }

  void Read() {
    state_ = READING;
    int result = trans_->Read(read_buf_, sizeof(read_buf_), this);
    if (result != net::ERR_IO_PENDING)
      DidRead(result);
  }

  enum State {
    IDLE,
    STARTING,
    READING,
    DONE
  } state_;

  scoped_ptr<net::HttpTransaction> trans_;
  std::string content_;
  char read_buf_[1024];
  int error_;

  static int quit_counter_;
};

//-----------------------------------------------------------------------------
// mock network layer

// This transaction class inspects the available set of mock transactions to
// find data for the request URL.  It supports IO operations that complete
// synchronously or asynchronously to help exercise different code paths in the
// HttpCache implementation.
class MockNetworkTransaction : public net::HttpTransaction {
 public:
  MockNetworkTransaction() :
      ALLOW_THIS_IN_INITIALIZER_LIST(task_factory_(this)), data_cursor_(0) {
  }

  virtual int Start(const net::HttpRequestInfo* request,
                    net::CompletionCallback* callback) {
    const MockTransaction* t = FindMockTransaction(request->url);
    if (!t)
      return net::ERR_FAILED;

    std::string resp_status = t->status;
    std::string resp_headers = t->response_headers;
    std::string resp_data = t->data;
    if (t->handler)
      (t->handler)(request, &resp_status, &resp_headers, &resp_data);

    std::string header_data =
        StringPrintf("%s\n%s\n", resp_status.c_str(), resp_headers.c_str());
    std::replace(header_data.begin(), header_data.end(), '\n', '\0');

    response_.request_time = base::Time::Now();
    response_.was_cached = false;
    response_.response_time = base::Time::Now();
    response_.headers = new net::HttpResponseHeaders(header_data);
    response_.ssl_info.cert_status = t->cert_status;
    data_ = resp_data;
    test_mode_ = t->test_mode;

    if (test_mode_ & TEST_MODE_SYNC_NET_START)
      return net::OK;

    CallbackLater(callback, net::OK);
    return net::ERR_IO_PENDING;
  }

  virtual int RestartIgnoringLastError(net::CompletionCallback* callback) {
    return net::ERR_FAILED;
  }

  virtual int RestartWithAuth(const std::wstring& username,
                              const std::wstring& password,
                              net::CompletionCallback* callback) {
    return net::ERR_FAILED;
  }

  virtual int Read(char* buf, int buf_len, net::CompletionCallback* callback) {
    int data_len = static_cast<int>(data_.size());
    int num = std::min(buf_len, data_len - data_cursor_);
    if (num) {
      memcpy(buf, data_.data() + data_cursor_, num);
      data_cursor_ += num;
    }
    if (test_mode_ & TEST_MODE_SYNC_NET_READ)
      return num;

    CallbackLater(callback, num);
    return net::ERR_IO_PENDING;
  }

  virtual const net::HttpResponseInfo* GetResponseInfo() const {
    return &response_;
  }

  virtual net::LoadState GetLoadState() const {
    NOTREACHED() << "define some mock state transitions";
    return net::LOAD_STATE_IDLE;
  }

  virtual uint64 GetUploadProgress() const {
    return 0;
  }

 private:
  void CallbackLater(net::CompletionCallback* callback, int result) {
    MessageLoop::current()->PostTask(FROM_HERE, task_factory_.NewRunnableMethod(
        &MockNetworkTransaction::RunCallback, callback, result));
  }
  void RunCallback(net::CompletionCallback* callback, int result) {
    callback->Run(result);
  }

  ScopedRunnableMethodFactory<MockNetworkTransaction> task_factory_;
  net::HttpResponseInfo response_;
  std::string data_;
  int data_cursor_;
  int test_mode_;
};

class MockNetworkLayer : public net::HttpTransactionFactory {
 public:
  MockNetworkLayer() : transaction_count_(0) {
  }

  virtual net::HttpTransaction* CreateTransaction() {
    transaction_count_++;
    return new MockNetworkTransaction();
  }

  virtual net::HttpCache* GetCache() {
    return NULL;
  }

  virtual void Suspend(bool suspend) {}

  int transaction_count() const { return transaction_count_; }

 private:
  int transaction_count_;
};


//-----------------------------------------------------------------------------
// helpers

// read the transaction completely
int ReadTransaction(net::HttpTransaction* trans, std::string* result);

#endif  // NET_HTTP_HTTP_TRANSACTION_UNITTEST_H_
