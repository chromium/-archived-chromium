// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "net/base/client_socket_factory.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------

namespace {

struct MockConnect {
  bool async;
  int result;
};

struct MockRead {
  bool async;
  int result;  // Ignored if data is non-null.
  const char* data;
  int data_len;  // -1 if strlen(data) should be used.
};

struct MockSocket {
  MockConnect connect;
  MockRead* reads;  // Terminated by a MockRead element with data == NULL.
};

// Holds an array of MockSocket elements.  As MockTCPClientSocket objects get
// instantiated, they take their data from the i'th element of this array.
//
// Tests should assign the first N entries of mock_sockets to point to valid
// MockSocket objects.  The first unused entry should be NULL'd.
//
MockSocket* mock_sockets[10];

// Index of the next mock_sockets element to use.
int mock_sockets_index;

class MockTCPClientSocket : public net::ClientSocket {
 public:
  MockTCPClientSocket(const net::AddressList& addresses)
#pragma warning(suppress:4355)
      : data_(mock_sockets[mock_sockets_index++]),
        method_factory_(this),
        callback_(NULL),
        read_index_(0),
        read_offset_(0),
        connected_(false) {
    DCHECK(data_) << "overran mock_sockets array";
  }
  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback) {
    DCHECK(!callback_);
    if (connected_)
      return net::OK;
    connected_ = true;
    if (data_->connect.async) {
      RunCallbackAsync(callback, data_->connect.result);
      return net::ERR_IO_PENDING;
    }
    return data_->connect.result;
  }
  virtual int ReconnectIgnoringLastError(net::CompletionCallback* callback) {
    NOTREACHED();
    return net::ERR_FAILED;
  }
  virtual void Disconnect() {
    connected_ = false;
    callback_ = NULL;
  }
  virtual bool IsConnected() const {
    return connected_;
  }
  // Socket methods:
  virtual int Read(char* buf, int buf_len, net::CompletionCallback* callback) {
    DCHECK(!callback_);
    MockRead& r = data_->reads[read_index_];
    int result;
    if (r.data) {
      if (r.data_len == -1)
        r.data_len = static_cast<int>(strlen(r.data));
      if (r.data_len - read_offset_ > 0) {
        result = std::min(buf_len, r.data_len - read_offset_);
        memcpy(buf, r.data + read_offset_, result);
        read_offset_ += result;
        if (read_offset_ == r.data_len) {
          read_index_++;
          read_offset_ = 0;
        }
      } else {
        result = 0;  // EOF
      }
    } else {
      result = r.result;
    }
    if (r.async) {
      RunCallbackAsync(callback, result);
      return net::ERR_IO_PENDING;
    }
    return result;
  }
  virtual int Write(const char* buf, int buf_len,
                    net::CompletionCallback* callback) {
    DCHECK(!callback_);
    return buf_len;  // OK, we wrote it.
  }
 private:
  void RunCallbackAsync(net::CompletionCallback* callback, int result) {
    callback_ = callback;
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &MockTCPClientSocket::RunCallback, result));
  }
  void RunCallback(int result) {
    net::CompletionCallback* c = callback_;
    callback_ = NULL;
    if (c)
      c->Run(result);
  }
  MockSocket* data_;
  ScopedRunnableMethodFactory<MockTCPClientSocket> method_factory_;
  net::CompletionCallback* callback_;
  int read_index_;
  int read_offset_;
  bool connected_;
};

class MockClientSocketFactory : public net::ClientSocketFactory {
 public:
  virtual net::ClientSocket* CreateTCPClientSocket(
      const net::AddressList& addresses) {
    return new MockTCPClientSocket(addresses);
  }
  virtual net::ClientSocket* CreateSSLClientSocket(
      net::ClientSocket* transport_socket,
      const std::string& hostname) {
    return NULL;
  }
};

MockClientSocketFactory mock_socket_factory;

class NullProxyResolver : public net::HttpProxyResolver {
 public:
  virtual int GetProxyConfig(net::HttpProxyConfig* config) {
    return net::ERR_FAILED;
  }
  virtual int GetProxyForURL(const std::wstring& query_url,
                             const std::wstring& pac_url,
                             net::HttpProxyInfo* results) {
    return net::ERR_FAILED;
  }
};

net::HttpNetworkSession* CreateSession() {
  return new net::HttpNetworkSession(new NullProxyResolver());
}

class HttpNetworkTransactionTest : public testing::Test {
 public:
  virtual void SetUp() {
    mock_sockets[0] = NULL;
    mock_sockets_index = 0;
  }
};

}  // namespace

//-----------------------------------------------------------------------------

TEST_F(HttpNetworkTransactionTest, Basic) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);
  trans->Destroy();
}

TEST_F(HttpNetworkTransactionTest, SimpleGET) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    { true, 0, "HTTP/1.0 200 OK\r\n\r\n", -1 },
    { true, 0, "hello world", -1 },
    { false, net::OK, NULL, 0 },
  };
  MockSocket data;
  data.connect.async = true;
  data.connect.result = net::OK;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->headers->GetStatusLine() == "HTTP/1.0 200 OK");

  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(response_data == "hello world");

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
}

TEST_F(HttpNetworkTransactionTest, ReuseConnection) {
  scoped_refptr<net::HttpNetworkSession> session = CreateSession();

  MockRead data_reads[] = {
    { true, 0, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", -1 },
    { true, 0, "hello", -1 },
    { true, 0, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", -1 },
    { true, 0, "world", -1 },
    { false, net::OK, NULL, 0 },
  };
  MockSocket data;
  data.connect.async = true;
  data.connect.result = net::OK;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  const char* kExpectedResponseData[] = {
    "hello", "world"
  };

  for (int i = 0; i < 2; ++i) {
    net::HttpTransaction* trans =
        new net::HttpNetworkTransaction(session, &mock_socket_factory);

    net::HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_TRUE(response->headers->GetStatusLine() == "HTTP/1.1 200 OK");

    std::string response_data;
    rv = ReadTransaction(trans, &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(response_data == kExpectedResponseData[i]);

    trans->Destroy();

    // Empty the current queue.
    MessageLoop::current()->Quit();
    MessageLoop::current()->Run();
  }
}

TEST_F(HttpNetworkTransactionTest, Ignores100) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data = new net::UploadData;
  request.upload_data->AppendBytes("foo", 3);
  request.load_flags = 0;

  MockRead data_reads[] = {
    { true, 0, "HTTP/1.0 100 Continue\r\n\r\n", -1 },
    { true, 0, "HTTP/1.0 200 OK\r\n\r\n", -1 },
    { true, 0, "hello world", -1 },
    { false, net::OK, NULL, 0 },
  };
  MockSocket data;
  data.connect.async = true;
  data.connect.result = net::OK;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  EXPECT_TRUE(response->headers->GetStatusLine() == "HTTP/1.0 200 OK");

  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_TRUE(response_data == "hello world");

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->Quit();
  MessageLoop::current()->Run();
}

TEST_F(HttpNetworkTransactionTest, KeepAliveConnectionReset) {
  scoped_refptr<net::HttpNetworkSession> session = CreateSession();

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data1_reads[] = {
    { true, 0, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", -1 },
    { true, 0, "hello", -1 },
    { true, net::ERR_CONNECTION_RESET, NULL, 0 },
  };
  MockSocket data1;
  data1.connect.async = true;
  data1.connect.result = net::OK;
  data1.reads = data1_reads;
  mock_sockets[0] = &data1;

  MockRead data2_reads[] = {
    { true, 0, "HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n", -1 },
    { true, 0, "world", -1 },
    { true, net::OK, NULL, 0 },
  };
  MockSocket data2;
  data2.connect.async = true;
  data2.connect.result = net::OK;
  data2.reads = data2_reads;
  mock_sockets[1] = &data2;

  const char* kExpectedResponseData[] = {
    "hello", "world"
  };

  for (int i = 0; i < 2; ++i) {
    TestCompletionCallback callback;

    net::HttpTransaction* trans =
        new net::HttpNetworkTransaction(session, &mock_socket_factory);

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_TRUE(response->headers->GetStatusLine() == "HTTP/1.1 200 OK");

    std::string response_data;
    rv = ReadTransaction(trans, &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_TRUE(response_data == kExpectedResponseData[i]);

    trans->Destroy();

    // Empty the current queue.
    MessageLoop::current()->Quit();
    MessageLoop::current()->Run();
  }
}
