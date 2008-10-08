// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/platform_test.h"
#include "net/base/client_socket_factory.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------


struct MockConnect {
  bool async;
  int result;

  // Asynchronous connection success.
  MockConnect() : async(true), result(net::OK) { }
};

struct MockRead {
  // Read failure (no data).
  MockRead(bool async, int result) : async(async) , result(result), data(NULL),
      data_len(0) { }

  // Asynchronous read success (inferred data length).
  MockRead(const char* data) : async(true),  result(0), data(data),
      data_len(strlen(data)) { }

  // Read success (inferred data length).
  MockRead(bool async, const char* data) : async(async), result(0), data(data),
      data_len(strlen(data)) { }

  // Read success.
  MockRead(bool async, const char* data, int data_len) : async(async),
      result(0), data(data), data_len(data_len) { }

  bool async;
  int result;
  const char* data;
  int data_len;
};

struct MockSocket {
  MockSocket() : reads(NULL) { }

  MockConnect connect;
  MockRead* reads;
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
      : data_(mock_sockets[mock_sockets_index++]),
        ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
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
      const std::string& hostname,
      int protocol_version_mask) {
    return NULL;
  }
};

MockClientSocketFactory mock_socket_factory;

class NullProxyResolver : public net::ProxyResolver {
 public:
  virtual int GetProxyConfig(net::ProxyConfig* config) {
    return net::ERR_FAILED;
  }
  virtual int GetProxyForURL(const std::string& query_url,
                             const std::string& pac_url,
                             net::ProxyInfo* results) {
    return net::ERR_FAILED;
  }
};

net::HttpNetworkSession* CreateSession() {
  return new net::HttpNetworkSession(new NullProxyResolver());
}

class HttpNetworkTransactionTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();
    mock_sockets[0] = NULL;
    mock_sockets_index = 0;
  }

 protected:
  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);
};

struct SimpleGetHelperResult {
  std::string status_line;
  std::string response_data;
};

SimpleGetHelperResult SimpleGetHelper(MockRead data_reads[]) {
  SimpleGetHelperResult out;

  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockSocket data;
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
  out.status_line = response->headers->GetStatusLine();

  rv = ReadTransaction(trans, &out.response_data);
  EXPECT_EQ(net::OK, rv);

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();

  return out;
}

//-----------------------------------------------------------------------------

TEST_F(HttpNetworkTransactionTest, Basic) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);
  trans->Destroy();
}

TEST_F(HttpNetworkTransactionTest, SimpleGET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/1.0 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
}

// Response with no status line.
TEST_F(HttpNetworkTransactionTest, SimpleGETNoHeaders) {
  MockRead data_reads[] = {
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk2Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk4Bytes) {
  MockRead data_reads[] = {
    MockRead("\n\nQJHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Beyond 4 bytes of slop and it should fail to find a status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk5Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxxxHTTP/1.1 404 Not Found\nServer: blah"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("xxxxxHTTP/1.1 404 Not Found\nServer: blah", out.response_data);
}

// Same as StatusLineJunk4Bytes, except the read chunks are smaller.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk4Bytes_Slow) {
  MockRead data_reads[] = {
    MockRead("\n"),
    MockRead("\n"),
    MockRead("Q"),
    MockRead("J"),
    MockRead("HTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Close the connection before enough bytes to have a status line.
TEST_F(HttpNetworkTransactionTest, StatusLinePartial) {
  MockRead data_reads[] = {
    MockRead("HTT"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("HTT", out.response_data);
}

// Simulate a 204 response, lacking a Content-Length header, sent over a
// persistent connection.  The response should still terminate since a 204
// cannot have a response body.
TEST_F(HttpNetworkTransactionTest, StopsReading204) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 204 No Content\r\n\r\n"),
    MockRead("junk"),  // Should not be read!!
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/1.1 204 No Content", out.status_line);
  EXPECT_EQ("", out.response_data);
}

TEST_F(HttpNetworkTransactionTest, ReuseConnection) {
  scoped_refptr<net::HttpNetworkSession> session = CreateSession();

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(false, net::OK),
  };
  MockSocket data;
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
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans, &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);

    trans->Destroy();

    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
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
    MockRead("HTTP/1.0 100 Continue\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  MockSocket data;
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
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans, &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("hello world", response_data);

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}

// read_failure specifies a read failure that should cause the network
// transaction to resend the request.
void HttpNetworkTransactionTest::KeepAliveConnectionResendRequestTest(
    const MockRead& read_failure) {
  scoped_refptr<net::HttpNetworkSession> session = CreateSession();

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data1_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    read_failure,  // Now, we reuse the connection and fail the first read.
  };
  MockSocket data1;
  data1.reads = data1_reads;
  mock_sockets[0] = &data1;

  MockRead data2_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(true, net::OK),
  };
  MockSocket data2;
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
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans, &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);

    trans->Destroy();

    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
  }
}

TEST_F(HttpNetworkTransactionTest, KeepAliveConnectionReset) {
  MockRead read_failure(true, net::ERR_CONNECTION_RESET);
  KeepAliveConnectionResendRequestTest(read_failure);
}

TEST_F(HttpNetworkTransactionTest, KeepAliveConnectionEOF) {
  MockRead read_failure(false, net::OK);  // EOF
  KeepAliveConnectionResendRequestTest(read_failure);
}

TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionReset) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead(true, net::ERR_CONNECTION_RESET),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  MockSocket data;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::ERR_CONNECTION_RESET, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}

// What do various browsers do when the server closes a non-keepalive
// connection without sending any response header or body?
//
// IE7: error page
// Safari 3.1.2 (Windows): error page
// Firefox 3.0.1: blank page
// Opera 9.52: after five attempts, blank page
// Us with WinHTTP: error page (net::ERR_INVALID_RESPONSE)
// Us: blank page
TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionEOF) {
  MockRead data_reads[] = {
    MockRead(false, net::OK),  // EOF
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("", out.response_data);
}
