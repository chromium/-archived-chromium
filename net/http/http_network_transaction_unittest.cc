// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>  // ceil

#include "base/compiler_specific.h"
#include "net/base/client_socket_factory.h"
#include "net/base/completion_callback.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_info.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

// TODO(eroman): Now that this is inside the net namespace, remove the redundant
// net:: qualifiers.

struct MockConnect {
  // Asynchronous connection success.
  MockConnect() : async(true), result(OK) { }
  MockConnect(bool a, int r) : async(a), result(r) { }

  bool async;
  int result;
};

struct MockRead {
  // Read failure (no data).
  MockRead(bool async, int result) : async(async) , result(result), data(NULL),
      data_len(0) { }

  // Asynchronous read success (inferred data length).
  explicit MockRead(const char* data) : async(true),  result(0), data(data),
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

// MockWrite uses the same member fields as MockRead, but with different
// meanings. The expected input to MockTCPClientSocket::Write() is given
// by {data, data_len}, and the return value of Write() is controlled by
// {async, result}.
typedef MockRead MockWrite;

struct MockSocket {
  MockSocket() : reads(NULL), writes(NULL) { }
  MockSocket(MockRead* r, MockWrite* w) : reads(r), writes(w) { }

  MockConnect connect;
  MockRead* reads;
  MockWrite* writes;
};

// Holds an array of MockSocket elements.  As MockTCPClientSocket objects get
// instantiated, they take their data from the i'th element of this array.
//
// Tests should assign the first N entries of mock_sockets to point to valid
// MockSocket objects.  The first unused entry should be NULL'd.
//
MockSocket* mock_sockets[10];

// MockSSLSockets only need to keep track of the return code from calls to
// Connect().
struct  MockSSLSocket {
  MockSSLSocket(bool async, int result) : connect(async, result) { }

  MockConnect connect;
};
MockSSLSocket* mock_ssl_sockets[10];

// Index of the next mock_sockets element to use.
int mock_sockets_index;
int mock_ssl_sockets_index;

class MockClientSocket : public SSLClientSocket {
 public:
  explicit MockClientSocket()
      : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
        callback_(NULL),
        connected_(false) {
  }

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) = 0;

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info) {
    NOTREACHED();
  }

  virtual void Disconnect() {
    connected_ = false;
    callback_ = NULL;
  }
  virtual bool IsConnected() const {
    return connected_;
  }
  virtual bool IsConnectedAndIdle() const {
    return connected_;
  }
  // Socket methods:
  virtual int Read(char* buf, int buf_len,
                   CompletionCallback* callback) = 0;
  virtual int Write(const char* buf, int buf_len,
                    CompletionCallback* callback) = 0;

#if defined(OS_LINUX)
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen) {
    memset(reinterpret_cast<char *>(name), 0, *namelen);
    return OK;
  }
#endif


 protected:
  void RunCallbackAsync(CompletionCallback* callback, int result) {
    callback_ = callback;
    MessageLoop::current()->PostTask(FROM_HERE,
        method_factory_.NewRunnableMethod(
            &MockClientSocket::RunCallback, result));
  }

  void RunCallback(int result) {
    CompletionCallback* c = callback_;
    callback_ = NULL;
    if (c)
      c->Run(result);
  }

  ScopedRunnableMethodFactory<MockClientSocket> method_factory_;
  CompletionCallback* callback_;
  bool connected_;
};

class MockTCPClientSocket : public MockClientSocket {
 public:
  explicit MockTCPClientSocket(const AddressList& addresses)
      : data_(mock_sockets[mock_sockets_index++]),
        read_index_(0),
        read_offset_(0),
        write_index_(0) {
    DCHECK(data_) << "overran mock_sockets array";
  }

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback) {
    DCHECK(!callback_);
    if (connected_)
      return OK;
    connected_ = true;
    if (data_->connect.async) {
      RunCallbackAsync(callback, data_->connect.result);
      return ERR_IO_PENDING;
    }
    return data_->connect.result;
  }

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback) {
    DCHECK(!callback_);
    MockRead& r = data_->reads[read_index_];
    int result = r.result;
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
    }
    if (r.async) {
      RunCallbackAsync(callback, result);
      return ERR_IO_PENDING;
    }
    return result;
  }

  virtual int Write(const char* buf, int buf_len,
                    CompletionCallback* callback) {
    DCHECK(buf);
    DCHECK(buf_len > 0);
    DCHECK(!callback_);
    // Not using mock writes; succeed synchronously.
    if (!data_->writes)
      return buf_len;

    // Check that what we are writing matches the expectation.
    // Then give the mocked return value.
    MockWrite& w = data_->writes[write_index_++];
    int result = w.result;
    if (w.data) {
      std::string expected_data(w.data, w.data_len);
      std::string actual_data(buf, buf_len);
      EXPECT_EQ(expected_data, actual_data);
      if (expected_data != actual_data)
        return ERR_UNEXPECTED;
      if (result == OK)
        result = w.data_len;
    }
    if (w.async) {
      RunCallbackAsync(callback, result);
      return ERR_IO_PENDING;
    }
    return result;
  }

 private:
  MockSocket* data_;
  int read_index_;
  int read_offset_;
  int write_index_;
};

class MockSSLClientSocket : public MockClientSocket {
 public:
  explicit MockSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config)
      : transport_(transport_socket),
        data_(mock_ssl_sockets[mock_ssl_sockets_index++]) {
    DCHECK(data_) << "overran mock_ssl_sockets array";
  }

  ~MockSSLClientSocket() {
    Disconnect();
  }

  virtual void GetSSLInfo(SSLInfo* ssl_info) {
    ssl_info->Reset();
  }

  friend class ConnectCallback;
  class ConnectCallback :
      public CompletionCallbackImpl<ConnectCallback> {
   public:
    ConnectCallback(MockSSLClientSocket *ssl_client_socket,
                    CompletionCallback* user_callback,
                    int rv)
        : ALLOW_THIS_IN_INITIALIZER_LIST(
              CompletionCallbackImpl<ConnectCallback>(
                  this, &ConnectCallback::Wrapper)),
          ssl_client_socket_(ssl_client_socket),
          user_callback_(user_callback),
          rv_(rv) {
    }

   private:
    void Wrapper(int rv) {
      if (rv_ == OK)
        ssl_client_socket_->connected_ = true;
      user_callback_->Run(rv_);
      delete this;
    }

    MockSSLClientSocket* ssl_client_socket_;
    CompletionCallback* user_callback_;
    int rv_;
  };

  virtual int Connect(CompletionCallback* callback) {
    DCHECK(!callback_);
    ConnectCallback* connect_callback = new ConnectCallback(
        this, callback, data_->connect.result);
    int rv = transport_->Connect(connect_callback);
    if (rv == OK) {
      delete connect_callback;
      if (data_->connect.async) {
        RunCallbackAsync(callback, data_->connect.result);
        return ERR_IO_PENDING;
      }
      if (data_->connect.result == OK)
        connected_ = true;
      return data_->connect.result;
    }
    return rv;
  }

  virtual void Disconnect() {
    MockClientSocket::Disconnect();
    if (transport_ != NULL)
      transport_->Disconnect();
  }

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback) {
    DCHECK(!callback_);
    return transport_->Read(buf, buf_len, callback);
  }

  virtual int Write(const char* buf, int buf_len,
                    CompletionCallback* callback) {
    DCHECK(!callback_);
    return transport_->Write(buf, buf_len, callback);
  }

 private:
  scoped_ptr<ClientSocket> transport_;
  MockSSLSocket* data_;
};

class MockClientSocketFactory : public ClientSocketFactory {
 public:
  virtual ClientSocket* CreateTCPClientSocket(
      const AddressList& addresses) {
    return new MockTCPClientSocket(addresses);
  }
  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config) {
    return new MockSSLClientSocket(transport_socket, hostname, ssl_config);
  }
};

MockClientSocketFactory mock_socket_factory;

// Create a proxy service which fails on all requests (falls back to direct).
net::ProxyService* CreateNullProxyService() {
  return net::ProxyService::CreateNull();
}

net::ProxyService* CreateFixedProxyService(const std::string& proxy) {
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy(proxy);
  return net::ProxyService::Create(&proxy_info);
}


net::HttpNetworkSession* CreateSession(net::ProxyService* proxy_service) {
  return new net::HttpNetworkSession(proxy_service);
}

class HttpNetworkTransactionTest : public PlatformTest {
 public:
  virtual void SetUp() {
    PlatformTest::SetUp();
    mock_sockets[0] = NULL;
    mock_sockets_index = 0;
    mock_ssl_sockets_index = 0;
  }

  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

 protected:
  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);
};

struct SimpleGetHelperResult {
  int rv;
  std::string status_line;
  std::string response_data;
};

SimpleGetHelperResult SimpleGetHelper(MockRead data_reads[]) {
  SimpleGetHelperResult out;

  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

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

  out.rv = callback.WaitForResult();
  if (out.rv != net::OK)
    return out;

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  out.status_line = response->headers->GetStatusLine();

  rv = ReadTransaction(trans.get(), &out.response_data);
  EXPECT_EQ(net::OK, rv);

  return out;
}

// Fill |str| with a long header list that consumes >= |size| bytes.
void FillLargeHeadersString(std::string* str, int size) {
  const char* row =
      "SomeHeaderName: xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\r\n";
  const int sizeof_row = strlen(row);
  const int num_rows = static_cast<int>(
      ceil(static_cast<float>(size) / sizeof_row));
  const int sizeof_data = num_rows * sizeof_row;
  DCHECK(sizeof_data >= size);
  str->reserve(sizeof_data);

  for (int i = 0; i < num_rows; ++i)
    str->append(row, sizeof_row);
}

// Alternative functions that eliminate randomness and dependency on the local
// host name so that the generated NTLM messages are reproducible.
void MockGenerateRandom1(uint8* output, size_t n) {
  static const uint8 bytes[] = {
    0x55, 0x29, 0x66, 0x26, 0x6b, 0x9c, 0x73, 0x54
  };
  static size_t current_byte = 0;
  for (size_t i = 0; i < n; ++i) {
    output[i] = bytes[current_byte++];
    current_byte %= arraysize(bytes);
  }
}

void MockGenerateRandom2(uint8* output, size_t n) {
  static const uint8 bytes[] = {
    0x96, 0x79, 0x85, 0xe7, 0x49, 0x93, 0x70, 0xa1,
    0x4e, 0xe7, 0x87, 0x45, 0x31, 0x5b, 0xd3, 0x1f
  };
  static size_t current_byte = 0;
  for (size_t i = 0; i < n; ++i) {
    output[i] = bytes[current_byte++];
    current_byte %= arraysize(bytes);
  }
}

std::string MockGetHostName() {
  return "WTC-WIN7";
}

//-----------------------------------------------------------------------------

TEST_F(HttpNetworkTransactionTest, Basic) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));
}

TEST_F(HttpNetworkTransactionTest, SimpleGET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
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
  EXPECT_EQ(net::OK, out.rv);
  EXPECT_EQ("HTTP/1.1 204 No Content", out.status_line);
  EXPECT_EQ("", out.response_data);
}

// Do a request using the HEAD method. Verify that we don't try to read the
// message body (since HEAD has none).
TEST_F(HttpNetworkTransactionTest, Head) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("HEAD / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Server: Blah\r\n"),
    MockRead("Content-Length: 1234\r\n\r\n"),

    // No response body because the test stops reading here.
    MockRead(false, net::ERR_UNEXPECTED),  // Should not be reached.
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // Check that the headers got parsed.
  EXPECT_TRUE(response->headers != NULL);
  EXPECT_EQ(1234, response->headers->GetContentLength());
  EXPECT_EQ("HTTP/1.1 404 Not Found", response->headers->GetStatusLine());

  std::string server_header;
  void* iter = NULL;
  bool has_server_header = response->headers->EnumerateHeader(
      &iter, "Server", &server_header);
  EXPECT_TRUE(has_server_header);
  EXPECT_EQ("Blah", server_header);

  // Reading should give EOF right away, since there is no message body
  // (despite non-zero content-length).
  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("", response_data);
}

TEST_F(HttpNetworkTransactionTest, ReuseConnection) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_refptr<net::HttpNetworkSession> session =
      CreateSession(proxy_service.get());

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
    scoped_ptr<net::HttpTransaction> trans(
        new net::HttpNetworkTransaction(session, &mock_socket_factory));

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
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

TEST_F(HttpNetworkTransactionTest, Ignores100) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

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
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// This test is almost the same as Ignores100 above, but the response contains
// a 102 instead of a 100. Also, instead of HTTP/1.0 the response is
// HTTP/1.1.
TEST_F(HttpNetworkTransactionTest, Ignores1xx) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 102 Unspecified status code\r\n\r\n"),
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
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
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(net::OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// read_failure specifies a read failure that should cause the network
// transaction to resend the request.
void HttpNetworkTransactionTest::KeepAliveConnectionResendRequestTest(
    const MockRead& read_failure) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_refptr<net::HttpNetworkSession> session =
      CreateSession(proxy_service.get());

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

    scoped_ptr<net::HttpTransaction> trans(
        new net::HttpNetworkTransaction(session, &mock_socket_factory));

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
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
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

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
}

// What do various browsers do when the server closes a non-keepalive
// connection without sending any response header or body?
//
// IE7: error page
// Safari 3.1.2 (Windows): error page
// Firefox 3.0.1: blank page
// Opera 9.52: after five attempts, blank page
// Us with WinHTTP: error page (net::ERR_INVALID_RESPONSE)
// Us: error page (net::EMPTY_RESPONSE)
TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionEOF) {
  MockRead data_reads[] = {
    MockRead(false, net::OK),  // EOF
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(net::ERR_EMPTY_RESPONSE, out.rv);
}

// Test the request-challenge-retry sequence for basic auth.
// (basic auth is the easiest to mock, because it has no randomness).
TEST_F(HttpNetworkTransactionTest, BasicAuth) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    // Give a couple authenticate options (only the middle one is actually
    // supported).
    MockRead("WWW-Authenticate: Basic\r\n"),  // Malformed
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("WWW-Authenticate: UNSUPPORTED realm=\"FOO\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(false, net::ERR_FAILED),
  };

  // After calling trans->RestartWithAuth(), this is the request we should
  // be issuing -- the final header line contains the credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;
  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAlive) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 14\r\n\r\n"),
    MockRead("Unauthorized\r\n"),

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with no response body to drain.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAliveNoBody) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Respond with 5 kb of response body.
  std::string large_body_string("Unauthorized");
  large_body_string.append(5 * 1024, ' ');
  large_body_string.append("\r\n");

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 0\r\n\r\n"),

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with a large response body to drain.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAliveLargeBody) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Respond with 5 kb of response body.
  std::string large_body_string("Unauthorized");
  large_body_string.append(5 * 1024, ' ');
  large_body_string.append("\r\n");

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // 5134 = 12 + 5 * 1024 + 2
    MockRead("Content-Length: 5134\r\n\r\n"),
    MockRead(true, large_body_string.data(), large_body_string.size()),

    // Lastly, the server responds with the actual content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// proxy connection, when setting up an SSL tunnel.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyKeepAlive) {
  // Configure against proxy server "myproxy:70".
  scoped_ptr<net::ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  scoped_refptr<net::HttpNetworkSession> session(
      CreateSession(proxy_service.get()));

  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      session.get(), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Authorization: Basic Zm9vOmJheg==\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407, using a persistent
  // connection.
  MockRead data_reads1[] = {
    // No credentials.
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead("0123456789"),

    // Wrong credentials (wrong password).
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    // No response body because the test stops reading here.
    MockRead(false, net::ERR_UNEXPECTED),  // Should not be reached.
  };

  MockSocket data1;
  data1.writes = data_writes1;
  data1.reads = data_reads1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(net::HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  // Wrong password (should be "bar").
  rv = trans->RestartWithAuth(L"foo", L"baz", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(net::HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);
}

// Test that we don't read the response body when we fail to establish a tunnel,
// even if the user cancels the proxy's auth attempt.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyCancelTunnel) {
  // Configure against proxy server "myproxy:70".
  scoped_ptr<net::ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  scoped_refptr<net::HttpNetworkSession> session(
      CreateSession(proxy_service.get()));

  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      session.get(), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407.
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(false, net::ERR_UNEXPECTED),  // Should not be reached.
  };

  MockSocket data;
  data.writes = data_writes;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(net::HttpVersion(1, 1) == response->headers->GetHttpVersion());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(net::ERR_TUNNEL_CONNECTION_FAILED, rv);
}

static void ConnectStatusHelperWithExpectedStatus(
    const MockRead& status, int expected_status) {
  // Configure against proxy server "myproxy:70".
  scoped_ptr<net::ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  scoped_refptr<net::HttpNetworkSession> session(
      CreateSession(proxy_service.get()));

  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      session.get(), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),
  };

  MockRead data_reads[] = {
    status,
    MockRead("Content-Length: 10\r\n\r\n"),
    // No response body because the test stops reading here.
    MockRead(false, net::ERR_UNEXPECTED),  // Should not be reached.
  };

  MockSocket data;
  data.writes = data_writes;
  data.reads = data_reads;
  mock_sockets[0] = &data;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(expected_status, rv);
}

static void ConnectStatusHelper(const MockRead& status) {
  ConnectStatusHelperWithExpectedStatus(
      status, net::ERR_TUNNEL_CONNECTION_FAILED);
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus100) {
  ConnectStatusHelper(MockRead("HTTP/1.1 100 Continue\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus101) {
  ConnectStatusHelper(MockRead("HTTP/1.1 101 Switching Protocols\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus201) {
  ConnectStatusHelper(MockRead("HTTP/1.1 201 Created\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus202) {
  ConnectStatusHelper(MockRead("HTTP/1.1 202 Accepted\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus203) {
  ConnectStatusHelper(
      MockRead("HTTP/1.1 203 Non-Authoritative Information\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus204) {
  ConnectStatusHelper(MockRead("HTTP/1.1 204 No Content\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus205) {
  ConnectStatusHelper(MockRead("HTTP/1.1 205 Reset Content\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus206) {
  ConnectStatusHelper(MockRead("HTTP/1.1 206 Partial Content\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus300) {
  ConnectStatusHelper(MockRead("HTTP/1.1 300 Multiple Choices\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus301) {
  ConnectStatusHelper(MockRead("HTTP/1.1 301 Moved Permanently\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus302) {
  ConnectStatusHelper(MockRead("HTTP/1.1 302 Found\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus303) {
  ConnectStatusHelper(MockRead("HTTP/1.1 303 See Other\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus304) {
  ConnectStatusHelper(MockRead("HTTP/1.1 304 Not Modified\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus305) {
  ConnectStatusHelper(MockRead("HTTP/1.1 305 Use Proxy\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus306) {
  ConnectStatusHelper(MockRead("HTTP/1.1 306\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus307) {
  ConnectStatusHelper(MockRead("HTTP/1.1 307 Temporary Redirect\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus400) {
  ConnectStatusHelper(MockRead("HTTP/1.1 400 Bad Request\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus401) {
  ConnectStatusHelper(MockRead("HTTP/1.1 401 Unauthorized\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus402) {
  ConnectStatusHelper(MockRead("HTTP/1.1 402 Payment Required\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus403) {
  ConnectStatusHelper(MockRead("HTTP/1.1 403 Forbidden\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus404) {
  ConnectStatusHelper(MockRead("HTTP/1.1 404 Not Found\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus405) {
  ConnectStatusHelper(MockRead("HTTP/1.1 405 Method Not Allowed\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus406) {
  ConnectStatusHelper(MockRead("HTTP/1.1 406 Not Acceptable\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus407) {
  ConnectStatusHelperWithExpectedStatus(
      MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
      net::ERR_PROXY_AUTH_REQUESTED);
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus408) {
  ConnectStatusHelper(MockRead("HTTP/1.1 408 Request Timeout\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus409) {
  ConnectStatusHelper(MockRead("HTTP/1.1 409 Conflict\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus410) {
  ConnectStatusHelper(MockRead("HTTP/1.1 410 Gone\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus411) {
  ConnectStatusHelper(MockRead("HTTP/1.1 411 Length Required\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus412) {
  ConnectStatusHelper(MockRead("HTTP/1.1 412 Precondition Failed\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus413) {
  ConnectStatusHelper(MockRead("HTTP/1.1 413 Request Entity Too Large\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus414) {
  ConnectStatusHelper(MockRead("HTTP/1.1 414 Request-URI Too Long\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus415) {
  ConnectStatusHelper(MockRead("HTTP/1.1 415 Unsupported Media Type\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus416) {
  ConnectStatusHelper(
      MockRead("HTTP/1.1 416 Requested Range Not Satisfiable\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus417) {
  ConnectStatusHelper(MockRead("HTTP/1.1 417 Expectation Failed\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus500) {
  ConnectStatusHelper(MockRead("HTTP/1.1 500 Internal Server Error\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus501) {
  ConnectStatusHelper(MockRead("HTTP/1.1 501 Not Implemented\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus502) {
  ConnectStatusHelper(MockRead("HTTP/1.1 502 Bad Gateway\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus503) {
  ConnectStatusHelper(MockRead("HTTP/1.1 503 Service Unavailable\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus504) {
  ConnectStatusHelper(MockRead("HTTP/1.1 504 Gateway Timeout\r\n"));
}

TEST_F(HttpNetworkTransactionTest, ConnectStatus505) {
  ConnectStatusHelper(MockRead("HTTP/1.1 505 HTTP Version Not Supported\r\n"));
}

// Test the flow when both the proxy server AND origin server require
// authentication. Again, this uses basic auth for both since that is
// the simplest to mock.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyThenServer) {
  scoped_ptr<net::ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  // Configure against proxy server "myproxy:70".
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()),
      &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 407 Unauthorized\r\n"),
    // Give a couple authenticate options (only the middle one is actually
    // supported).
    MockRead("Proxy-Authenticate: Basic\r\n"),  // Malformed
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Proxy-Authenticate: UNSUPPORTED realm=\"FOO\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    // Large content-length -- won't matter, as connection will be reset.
    MockRead("Content-Length: 10000\r\n\r\n"),
    MockRead(false, net::ERR_FAILED),
  };

  // After calling trans->RestartWithAuth() the first time, this is the
  // request we should be issuing -- the final header line contains the
  // proxy's credentials.
  MockWrite data_writes2[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  // Now the proxy server lets the request pass through to origin server.
  // The origin server responds with a 401.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    // Note: We are using the same realm-name as the proxy server. This is
    // completely valid, as realms are unique across hosts.
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 2000\r\n\r\n"),
    MockRead(false, net::ERR_FAILED),  // Won't be reached.
  };

  // After calling trans->RestartWithAuth() the second time, we should send
  // the credentials for both the proxy and origin server.
  MockWrite data_writes3[] = {
    MockWrite("GET http://www.google.com/ HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
              "Proxy-Authorization: Basic Zm9vOmJhcg==\r\n"
              "Authorization: Basic Zm9vMjpiYXIy\r\n\r\n"),
  };

  // Lastly we get the desired content.
  MockRead data_reads3[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;
  MockSocket data3;
  data3.reads = data_reads3;
  data3.writes = data_writes3;
  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = &data3;
  mock_sockets[3] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(L"foo2", L"bar2", &callback3);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// The NTLM authentication unit tests were generated by capturing the HTTP
// requests and responses using Fiddler 2 and inspecting the generated random
// bytes in the debugger.

// Enter the correct password and authenticate successfully.
TEST_F(HttpNetworkTransactionTest, NTLMAuth1) {
  net::HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom1,
                                                         MockGetHostName);

  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://172.22.68.17/kids/login.aspx");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    // Negotiate and NTLM are often requested together.  We only support NTLM.
    MockRead("WWW-Authenticate: Negotiate\r\n"),
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, net::ERR_UNEXPECTED),
  };

  MockWrite data_writes2[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwBVKW"
              "Yma5xzVAAAAAAAAAAAAAAAAAAAAACH+gWcm+YsP9Tqb9zCR3WAeZZX"
              "ahlhx5I=\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCjGpMpPGlYKkAAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;
  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"172.22.68.17", response->auth_challenge->host);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(L"testing-ntlm", L"testing-ntlm", &callback3);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}

// Enter a wrong password, and then the correct one.
TEST_F(HttpNetworkTransactionTest, NTLMAuth2) {
  net::HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom2,
                                                         MockGetHostName);

  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://172.22.68.17/kids/login.aspx");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    // Negotiate and NTLM are often requested together.  We only support NTLM.
    MockRead("WWW-Authenticate: Negotiate\r\n"),
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, net::ERR_UNEXPECTED),
  };

  MockWrite data_writes2[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwCWeY"
              "XnSZNwoQAAAAAAAAAAAAAAAAAAAADLa34/phTTKzNTWdub+uyFleOj"
              "4Ww7b7E=\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCbVWUZezVGpAAAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Wrong password.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: Negotiate\r\n"),
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, net::ERR_UNEXPECTED),
  };

  MockWrite data_writes3[] = {
    // After restarting with a null identity, this is the
    // request we should be issuing -- the final header line contains a Type
    // 1 message.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM "
              "TlRMTVNTUAABAAAAB4IIAAAAAAAAAAAAAAAAAAAAAAA=\r\n\r\n"),

    // After calling trans->RestartWithAuth(), we should send a Type 3 message
    // (the credentials for the origin server).  The second request continues
    // on the same connection.
    MockWrite("GET /kids/login.aspx HTTP/1.1\r\n"
              "Host: 172.22.68.17\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: NTLM TlRMTVNTUAADAAAAGAAYAGgAAAAYABgAgA"
              "AAAAAAAABAAAAAGAAYAEAAAAAQABAAWAAAAAAAAAAAAAAABYIIAHQA"
              "ZQBzAHQAaQBuAGcALQBuAHQAbABtAFcAVABDAC0AVwBJAE4ANwBO54"
              "dFMVvTHwAAAAAAAAAAAAAAAAAAAACS7sT6Uzw7L0L//WUqlIaVWpbI"
              "+4MUm7c=\r\n\r\n"),
  };

  MockRead data_reads3[] = {
    // The origin server responds with a Type 2 message.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: NTLM "
             "TlRMTVNTUAACAAAADAAMADgAAAAFgokCL24VN8dgOR8AAAAAAAAAALo"
             "AugBEAAAABQEoCgAAAA9HAE8ATwBHAEwARQACAAwARwBPAE8ARwBMAE"
             "UAAQAaAEEASwBFAEUAUwBBAFIAQQAtAEMATwBSAFAABAAeAGMAbwByA"
             "HAALgBnAG8AbwBnAGwAZQAuAGMAbwBtAAMAQABhAGsAZQBlAHMAYQBy"
             "AGEALQBjAG8AcgBwAC4AYQBkAC4AYwBvAHIAcAAuAGcAbwBvAGcAbAB"
             "lAC4AYwBvAG0ABQAeAGMAbwByAHAALgBnAG8AbwBnAGwAZQAuAGMAbw"
             "BtAAAAAAA=\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n"),
    MockRead("Proxy-Support: Session-Based-Authentication\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;
  MockSocket data3;
  data3.reads = data_reads3;
  data3.writes = data_writes3;
  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = &data3;
  mock_sockets[3] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"172.22.68.17", response->auth_challenge->host);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  // Enter the wrong password.
  rv = trans->RestartWithAuth(L"testing-ntlm", L"wrongpassword", &callback3);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback4;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback4);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = callback4.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  // TODO(eroman): this should really include the effective port (80)
  EXPECT_EQ(L"172.22.68.17", response->auth_challenge->host);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback5;

  // Now enter the right password.
  rv = trans->RestartWithAuth(L"testing-ntlm", L"testing-ntlm", &callback5);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback5.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}

// Test reading a server response which has only headers, and no body.
// After some maximum number of bytes is consumed, the transaction should
// fail with ERR_RESPONSE_HEADERS_TOO_BIG.
TEST_F(HttpNetworkTransactionTest, LargeHeadersNoBody) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  // Respond with 50 kb of headers (we should fail after 32 kb).
  std::string large_headers_string;
  FillLargeHeadersString(&large_headers_string, 50 * 1024);

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead(true, large_headers_string.data(), large_headers_string.size()),
    MockRead("\r\nBODY"),
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
  EXPECT_EQ(net::ERR_RESPONSE_HEADERS_TOO_BIG, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

// Make sure that we don't try to reuse a TCPClientSocket when failing to
// establish tunnel.
// http://code.google.com/p/chromium/issues/detail?id=3772
TEST_F(HttpNetworkTransactionTest, DontRecycleTCPSocketForSSLTunnel) {
  // Configure against proxy server "myproxy:70".
  scoped_ptr<net::ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  scoped_refptr<net::HttpNetworkSession> session(
      CreateSession(proxy_service.get()));

  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      session.get(), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),
  };

  // The proxy responds to the connect with a 404, using a persistent
  // connection. Usually a proxy would return 501 (not implemented),
  // or 200 (tunnel established).
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(false, net::ERR_UNEXPECTED),  // Should not be reached.
  };

  MockSocket data1;
  data1.writes = data_writes1;
  data1.reads = data_reads1;
  mock_sockets[0] = &data1;
  mock_sockets[1] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::ERR_TUNNEL_CONNECTION_FAILED, rv);

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);

  // We now check to make sure the TCPClientSocket was not added back to
  // the pool.
  EXPECT_EQ(0, session->connection_pool()->idle_socket_count());
  trans.reset();
  // Make sure that the socket didn't get recycled after calling the destructor.
  EXPECT_EQ(0, session->connection_pool()->idle_socket_count());
}

TEST_F(HttpNetworkTransactionTest, ResendRequestOnWriteBodyError) {
  net::HttpRequestInfo request[2];
  // Transaction 1: a GET request that succeeds.  The socket is recycled
  // after use.
  request[0].method = "GET";
  request[0].url = GURL("http://www.google.com/");
  request[0].load_flags = 0;
  // Transaction 2: a POST request.  Reuses the socket kept alive from
  // transaction 1.  The first attempts fails when writing the POST data.
  // This causes the transaction to retry with a new socket.  The second
  // attempt succeeds.
  request[1].method = "POST";
  request[1].url = GURL("http://www.google.com/login.cgi");
  request[1].upload_data = new net::UploadData;
  request[1].upload_data->AppendBytes("foo", 3);
  request[1].load_flags = 0;

  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_refptr<net::HttpNetworkSession> session =
      CreateSession(proxy_service.get());

  // The first socket is used for transaction 1 and the first attempt of
  // transaction 2.

  // The response of transaction 1.
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  // The mock write results of transaction 1 and the first attempt of
  // transaction 2.
  MockWrite data_writes1[] = {
    MockWrite(false, 64),  // GET
    MockWrite(false, 93),  // POST
    MockWrite(false, net::ERR_CONNECTION_ABORTED),  // POST data
  };
  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;

  // The second socket is used for the second attempt of transaction 2.

  // The response of transaction 2.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\n"),
    MockRead("welcome"),
    MockRead(false, net::OK),
  };
  // The mock write results of the second attempt of transaction 2.
  MockWrite data_writes2[] = {
    MockWrite(false, 93),  // POST
    MockWrite(false, 3),  // POST data
  };
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;

  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = NULL;

  const char* kExpectedResponseData[] = {
    "hello world", "welcome"
  };

  for (int i = 0; i < 2; ++i) {
    scoped_ptr<net::HttpTransaction> trans(
        new net::HttpNetworkTransaction(session, &mock_socket_factory));

    TestCompletionCallback callback;

    int rv = trans->Start(&request[i], &callback);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(net::OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

// Test the request-challenge-retry sequence for basic auth when there is
// an identity in the URL. The request should be sent as normal, but when
// it fails the identity from the URL is used to answer the challenge.
TEST_F(HttpNetworkTransactionTest, AuthIdentityInUrl) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  net::HttpRequestInfo request;
  request.method = "GET";
  // Note: the URL has a username:password in it.
  request.url = GURL("http://foo:bar@www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes1[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads1[] = {
    MockRead("HTTP/1.0 401 Unauthorized\r\n"),
    MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(false, net::ERR_FAILED),
  };

  // After the challenge above, the transaction will be restarted using the
  // identity from the url (foo, bar) to answer the challenge.
  MockWrite data_writes2[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
  };

  MockRead data_reads2[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, net::OK),
  };

  MockSocket data1;
  data1.reads = data_reads1;
  data1.writes = data_writes1;
  MockSocket data2;
  data2.reads = data_reads2;
  data2.writes = data_writes2;
  mock_sockets[0] = &data1;
  mock_sockets[1] = &data2;
  mock_sockets[2] = NULL;

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(net::OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(net::ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(net::OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const net::HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // There is no challenge info, since the identity in URL worked.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(100, response->headers->GetContentLength());

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}

// Test that previously tried username/passwords for a realm get re-used.
TEST_F(HttpNetworkTransactionTest, BasicAuthCacheAndPreauth) {
  scoped_ptr<net::ProxyService> proxy_service(CreateNullProxyService());
  scoped_refptr<net::HttpNetworkSession> session =
      CreateSession(proxy_service.get());

  // Transaction 1: authenticate (foo, bar) on MyRealm1
  {
    scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
        session, &mock_socket_factory));

    net::HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/y/z");
    request.load_flags = 0;

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(false, net::ERR_FAILED),
    };

    // Resend with authorization (username=foo, password=bar)
    MockWrite data_writes2[] = {
      MockWrite("GET /x/y/z HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(false, net::OK),
    };

    MockSocket data1;
    data1.reads = data_reads1;
    data1.writes = data_writes1;
    MockSocket data2;
    data2.reads = data_reads2;
    data2.writes = data_writes2;
    mock_sockets_index = 0;
    mock_sockets[0] = &data1;
    mock_sockets[1] = &data2;
    mock_sockets[2] = NULL;

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    // TODO(eroman): this should really include the effective port (80)
    EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
    EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 2: authenticate (foo2, bar2) on MyRealm2
  {
    scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
        session, &mock_socket_factory));

    net::HttpRequestInfo request;
    request.method = "GET";
    // Note that Transaction 1 was at /x/y/z, so this is in the same
    // protection space as MyRealm1.
    request.url = GURL("http://www.google.com/x/y/a/b");
    request.load_flags = 0;

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/a/b HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                // Send preemptive authorization for MyRealm1
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // The server didn't like the preemptive authorization, and
    // challenges us for a different realm (MyRealm2).
    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm2\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(false, net::ERR_FAILED),
    };

    // Resend with authorization for MyRealm2 (username=foo2, password=bar2)
    MockWrite data_writes2[] = {
      MockWrite("GET /x/y/a/b HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vMjpiYXIy\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(false, net::OK),
    };

    MockSocket data1;
    data1.reads = data_reads1;
    data1.writes = data_writes1;
    MockSocket data2;
    data2.reads = data_reads2;
    data2.writes = data_writes2;
    mock_sockets_index = 0;
    mock_sockets[0] = &data1;
    mock_sockets[1] = &data2;
    mock_sockets[2] = NULL;

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    // TODO(eroman): this should really include the effective port (80)
    EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
    EXPECT_EQ(L"MyRealm2", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(L"foo2", L"bar2", &callback2);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 3: Resend a request in MyRealm's protection space --
  // succeed with preemptive authorization.
  {
    scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
        session, &mock_socket_factory));

    net::HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/y/z2");
    request.load_flags = 0;

    MockWrite data_writes1[] = {
      MockWrite("GET /x/y/z2 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                // The authorization for MyRealm1 gets sent preemptively
                // (since the url is in the same protection space)
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the preemptive authorization
    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(false, net::OK),
    };

    MockSocket data1;
    data1.reads = data_reads1;
    data1.writes = data_writes1;
    mock_sockets_index = 0;
    mock_sockets[0] = &data1;
    mock_sockets[1] = NULL;

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 4: request another URL in MyRealm (however the
  // url is not known to belong to the protection space, so no pre-auth).
  {
    scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
        session, &mock_socket_factory));

    net::HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/x/1");
    request.load_flags = 0;

    MockWrite data_writes1[] = {
      MockWrite("GET /x/1 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(false, net::ERR_FAILED),
    };

    // Resend with authorization from MyRealm's cache.
    MockWrite data_writes2[] = {
      MockWrite("GET /x/1 HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(false, net::OK),
    };

    MockSocket data1;
    data1.reads = data_reads1;
    data1.writes = data_writes1;
    MockSocket data2;
    data2.reads = data_reads2;
    data2.writes = data_writes2;
    mock_sockets_index = 0;
    mock_sockets[0] = &data1;
    mock_sockets[1] = &data2;
    mock_sockets[2] = NULL;

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 5: request a URL in MyRealm, but the server rejects the
  // cached identity. Should invalidate and re-prompt.
  {
    scoped_ptr<net::HttpTransaction> trans(new net::HttpNetworkTransaction(
        session, &mock_socket_factory));

    net::HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/p/q/t");
    request.load_flags = 0;

    MockWrite data_writes1[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n\r\n"),
    };

    MockRead data_reads1[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(false, net::ERR_FAILED),
    };

    // Resend with authorization from cache for MyRealm.
    MockWrite data_writes2[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vOmJhcg==\r\n\r\n"),
    };

    // Sever rejects the authorization.
    MockRead data_reads2[] = {
      MockRead("HTTP/1.0 401 Unauthorized\r\n"),
      MockRead("WWW-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
      MockRead("Content-Length: 10000\r\n\r\n"),
      MockRead(false, net::ERR_FAILED),
    };

    // At this point we should prompt for new credentials for MyRealm.
    // Restart with username=foo3, password=foo4.
    MockWrite data_writes3[] = {
      MockWrite("GET /p/q/t HTTP/1.1\r\n"
                "Host: www.google.com\r\n"
                "Connection: keep-alive\r\n"
                "Authorization: Basic Zm9vMzpiYXIz\r\n\r\n"),
    };

    // Sever accepts the authorization.
    MockRead data_reads3[] = {
      MockRead("HTTP/1.0 200 OK\r\n"),
      MockRead("Content-Length: 100\r\n\r\n"),
      MockRead(false, net::OK),
    };

    MockSocket data1;
    data1.reads = data_reads1;
    data1.writes = data_writes1;
    MockSocket data2;
    data2.reads = data_reads2;
    data2.writes = data_writes2;
    MockSocket data3;
    data3.reads = data_reads3;
    data3.writes = data_writes3;
    mock_sockets_index = 0;
    mock_sockets[0] = &data1;
    mock_sockets[1] = &data2;
    mock_sockets[2] = &data3;
    mock_sockets[3] = NULL;

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(net::OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const net::HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    // TODO(eroman): this should really include the effective port (80)
    EXPECT_EQ(L"www.google.com", response->auth_challenge->host);
    EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback3;

    rv = trans->RestartWithAuth(L"foo3", L"bar3", &callback3);
    EXPECT_EQ(net::ERR_IO_PENDING, rv);

    rv = callback3.WaitForResult();
    EXPECT_EQ(net::OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }
}

// Test the ResetStateForRestart() private method.
TEST_F(HttpNetworkTransactionTest, ResetStateForRestart) {
  // Create a transaction (the dependencies aren't important).
  scoped_ptr<ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<HttpNetworkTransaction> trans(new HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  // Setup some state (which we expect ResetStateForRestart() will clear).
  trans->header_buf_.reset(static_cast<char*>(malloc(10)));
  trans->header_buf_capacity_ = 10;
  trans->header_buf_len_ = 3;
  trans->header_buf_body_offset_ = 11;
  trans->header_buf_http_offset_ = 0;
  trans->response_body_length_ = 100;
  trans->response_body_read_ = 1;
  trans->read_buf_ = new IOBuffer(15);
  trans->read_buf_len_ = 15;
  trans->request_headers_ = "Authorization: NTLM";
  trans->request_headers_bytes_sent_ = 3;

  // Setup state in response_
  trans->response_.auth_challenge = new AuthChallengeInfo();
  trans->response_.ssl_info.cert_status = -15;
  trans->response_.response_time = base::Time::Now();
  trans->response_.was_cached = true; // (Wouldn't ever actually be true...)

  { // Setup state for response_.vary_data
    HttpRequestInfo request;
    std::string temp("HTTP/1.1 200 OK\nVary: foo, bar\n\n");
    std::replace(temp.begin(), temp.end(), '\n', '\0');
    scoped_refptr<HttpResponseHeaders> response = new HttpResponseHeaders(temp);
    request.extra_headers = "Foo: 1\nbar: 23";
    EXPECT_TRUE(trans->response_.vary_data.Init(request, *response));
  }

  // Cause the above state to be reset.
  trans->ResetStateForRestart();

  // Verify that the state that needed to be reset, has been reset.
  EXPECT_EQ(NULL, trans->header_buf_.get());
  EXPECT_EQ(0, trans->header_buf_capacity_);
  EXPECT_EQ(0, trans->header_buf_len_);
  EXPECT_EQ(-1, trans->header_buf_body_offset_);
  EXPECT_EQ(-1, trans->header_buf_http_offset_);
  EXPECT_EQ(-1, trans->response_body_length_);
  EXPECT_EQ(0, trans->response_body_read_);
  EXPECT_EQ(NULL, trans->read_buf_.get());
  EXPECT_EQ(0, trans->read_buf_len_);
  EXPECT_EQ("", trans->request_headers_);
  EXPECT_EQ(0U, trans->request_headers_bytes_sent_);
  EXPECT_EQ(NULL, trans->response_.auth_challenge.get());
  EXPECT_EQ(NULL, trans->response_.headers.get());
  EXPECT_EQ(false, trans->response_.was_cached);
  EXPECT_EQ(base::kInvalidPlatformFileValue,
            trans->response_.response_data_file);
  EXPECT_EQ(0, trans->response_.ssl_info.cert_status);
  EXPECT_FALSE(trans->response_.vary_data.is_valid());
}

// Test HTTPS connections to a site with a bad certificate
TEST_F(HttpNetworkTransactionTest, HTTPSBadCertificate) {
  scoped_ptr<ProxyService> proxy_service(CreateNullProxyService());
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      CreateSession(proxy_service.get()), &mock_socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  MockSocket ssl_bad_certificate;
  MockSocket data(data_reads, data_writes);
  MockSSLSocket ssl_bad(true, ERR_CERT_AUTHORITY_INVALID);
  MockSSLSocket ssl(true, OK);

  mock_sockets[0] = &ssl_bad_certificate;
  mock_sockets[1] = &data;
  mock_sockets[2] = NULL;

  mock_ssl_sockets[0] = &ssl_bad;
  mock_ssl_sockets[1] = &ssl;
  mock_ssl_sockets[2] = NULL;

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, rv);

  rv = trans->RestartIgnoringLastError(&callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();

  EXPECT_FALSE(response == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test HTTPS connections to a site with a bad certificate, going through a
// proxy
TEST_F(HttpNetworkTransactionTest, HTTPSBadCertificateViaProxy) {
  scoped_ptr<ProxyService> proxy_service(
      CreateFixedProxyService("myproxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite proxy_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),
  };

  MockRead proxy_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead(false, net::OK)
  };

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n\r\n"),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  MockSocket ssl_bad_certificate(proxy_reads, proxy_writes);
  MockSocket data(data_reads, data_writes);
  MockSSLSocket ssl_bad(true, ERR_CERT_AUTHORITY_INVALID);
  MockSSLSocket ssl(true, OK);

  mock_sockets[0] = &ssl_bad_certificate;
  mock_sockets[1] = &data;
  mock_sockets[2] = NULL;

  mock_ssl_sockets[0] = &ssl_bad;
  mock_ssl_sockets[1] = &ssl;
  mock_ssl_sockets[2] = NULL;

  TestCompletionCallback callback;

  for (int i = 0; i < 2; i++) {
    mock_sockets_index = 0;
    mock_ssl_sockets_index = 0;

    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        CreateSession(proxy_service.get()), &mock_socket_factory));

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(ERR_CERT_AUTHORITY_INVALID, rv);

    rv = trans->RestartIgnoringLastError(&callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();

    EXPECT_FALSE(response == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }
}

}  // namespace net
