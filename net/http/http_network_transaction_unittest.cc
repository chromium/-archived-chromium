// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <math.h>  // ceil

#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/base/host_resolver_unittest.h"
#include "net/base/ssl_info.h"
#include "net/base/test_completion_callback.h"
#include "net/base/upload_data.h"
#include "net/http/http_auth_handler_ntlm.h"
#include "net/http/http_network_session.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_transaction_unittest.h"
#include "net/proxy/proxy_config_service_fixed.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/socket_test_util.h"
#include "net/socket/ssl_client_socket.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

//-----------------------------------------------------------------------------

namespace net {

// Create a proxy service which fails on all requests (falls back to direct).
ProxyService* CreateNullProxyService() {
  return ProxyService::CreateNull();
}

// Helper to manage the lifetimes of the dependencies for a
// HttpNetworkTransaction.
class SessionDependencies {
 public:
  // Default set of dependencies -- "null" proxy service.
  SessionDependencies() : proxy_service(CreateNullProxyService()) {}

  // Custom proxy service dependency.
  explicit SessionDependencies(ProxyService* proxy_service)
      : proxy_service(proxy_service) {}

  HostResolver host_resolver;
  scoped_ptr<ProxyService> proxy_service;
  MockClientSocketFactory socket_factory;
};

ProxyService* CreateFixedProxyService(const std::string& proxy) {
  net::ProxyConfig proxy_config;
  proxy_config.proxy_rules.ParseFromString(proxy);
  return ProxyService::CreateFixed(proxy_config);
}


HttpNetworkSession* CreateSession(SessionDependencies* session_deps) {
  return new HttpNetworkSession(&session_deps->host_resolver,
                                session_deps->proxy_service.get(),
                                &session_deps->socket_factory);
}

class HttpNetworkTransactionTest : public PlatformTest {
 public:
  virtual void TearDown() {
    // Empty the current queue.
    MessageLoop::current()->RunAllPending();
    PlatformTest::TearDown();
  }

 protected:
  void KeepAliveConnectionResendRequestTest(const MockRead& read_failure);

  struct SimpleGetHelperResult {
    int rv;
    std::string status_line;
    std::string response_data;
  };

  SimpleGetHelperResult SimpleGetHelper(MockRead data_reads[]) {
    SimpleGetHelperResult out;

    SessionDependencies session_deps;
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(
            CreateSession(&session_deps),
            &session_deps.socket_factory));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    StaticMockSocket data(data_reads, NULL);
    session_deps.socket_factory.AddMockSocket(&data);

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    out.rv = callback.WaitForResult();
    if (out.rv != OK)
      return out;

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    out.status_line = response->headers->GetStatusLine();

    rv = ReadTransaction(trans.get(), &out.response_data);
    EXPECT_EQ(OK, rv);

    return out;
  }

  void ConnectStatusHelperWithExpectedStatus(const MockRead& status,
                                             int expected_status);

  void ConnectStatusHelper(const MockRead& status);
};

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

class CaptureGroupNameSocketPool : public ClientSocketPool {
 public:
  CaptureGroupNameSocketPool() {
  }
  virtual int RequestSocket(const std::string& group_name,
                            const HostResolver::RequestInfo& resolve_info,
                            int priority,
                            ClientSocketHandle* handle,
                            CompletionCallback* callback) {
    last_group_name_ = group_name;
    return ERR_IO_PENDING;
  }

  const std::string last_group_name_received() const {
    return last_group_name_;
  }

  virtual void CancelRequest(const std::string& group_name,
                             const ClientSocketHandle* handle) { }
  virtual void ReleaseSocket(const std::string& group_name,
                             ClientSocket* socket) {}
  virtual void CloseIdleSockets() {}
  virtual HostResolver* GetHostResolver() const {
    return NULL;
  }
  virtual int IdleSocketCount() const {
    return 0;
  }
  virtual int IdleSocketCountInGroup(const std::string& group_name) const {
    return 0;
  }
  virtual LoadState GetLoadState(const std::string& group_name,
                                 const ClientSocketHandle* handle) const {
    return LOAD_STATE_IDLE;
  }
 protected:
  std::string last_group_name_;
};

//-----------------------------------------------------------------------------

TEST_F(HttpNetworkTransactionTest, Basic) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));
}

TEST_F(HttpNetworkTransactionTest, SimpleGET) {
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
}

// Response with no status line.
TEST_F(HttpNetworkTransactionTest, SimpleGETNoHeaders) {
  MockRead data_reads[] = {
    MockRead("hello world"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/0.9 200 OK", out.status_line);
  EXPECT_EQ("hello world", out.response_data);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk2Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Allow up to 4 bytes of junk to precede status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk4Bytes) {
  MockRead data_reads[] = {
    MockRead("\n\nQJHTTP/1.0 404 Not Found\nServer: blah\n\nDATA"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Beyond 4 bytes of slop and it should fail to find a status line.
TEST_F(HttpNetworkTransactionTest, StatusLineJunk5Bytes) {
  MockRead data_reads[] = {
    MockRead("xxxxxHTTP/1.1 404 Not Found\nServer: blah"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
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
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.0 404 Not Found", out.status_line);
  EXPECT_EQ("DATA", out.response_data);
}

// Close the connection before enough bytes to have a status line.
TEST_F(HttpNetworkTransactionTest, StatusLinePartial) {
  MockRead data_reads[] = {
    MockRead("HTT"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
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
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(OK, out.rv);
  EXPECT_EQ("HTTP/1.1 204 No Content", out.status_line);
  EXPECT_EQ("", out.response_data);
}

// Do a request using the HEAD method. Verify that we don't try to read the
// message body (since HEAD has none).
TEST_F(HttpNetworkTransactionTest, Head) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
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
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);
}

TEST_F(HttpNetworkTransactionTest, ReuseConnection) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session = CreateSession(&session_deps);

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(false, OK),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  const char* kExpectedResponseData[] = {
    "hello", "world"
  };

  for (int i = 0; i < 2; ++i) {
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(session, &session_deps.socket_factory));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL("http://www.google.com/");
    request.load_flags = 0;

    TestCompletionCallback callback;

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

TEST_F(HttpNetworkTransactionTest, Ignores100) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.foo.com/");
  request.upload_data = new UploadData;
  request.upload_data->AppendBytes("foo", 3);
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.0 100 Continue\r\n\r\n"),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, OK),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.0 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// This test is almost the same as Ignores100 above, but the response contains
// a 102 instead of a 100. Also, instead of HTTP/1.0 the response is
// HTTP/1.1.
TEST_F(HttpNetworkTransactionTest, Ignores1xx) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 102 Unspecified status code\r\n\r\n"),
    MockRead("HTTP/1.1 200 OK\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, OK),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);
}

// read_failure specifies a read failure that should cause the network
// transaction to resend the request.
void HttpNetworkTransactionTest::KeepAliveConnectionResendRequestTest(
    const MockRead& read_failure) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session = CreateSession(&session_deps);

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.foo.com/");
  request.load_flags = 0;

  MockRead data1_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("hello"),
    read_failure,  // Now, we reuse the connection and fail the first read.
  };
  StaticMockSocket data1(data1_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data1);

  MockRead data2_reads[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\n"),
    MockRead("world"),
    MockRead(true, OK),
  };
  StaticMockSocket data2(data2_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data2);

  const char* kExpectedResponseData[] = {
    "hello", "world"
  };

  for (int i = 0; i < 2; ++i) {
    TestCompletionCallback callback;

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(session, &session_deps.socket_factory));

    int rv = trans->Start(&request, &callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

TEST_F(HttpNetworkTransactionTest, KeepAliveConnectionReset) {
  MockRead read_failure(true, ERR_CONNECTION_RESET);
  KeepAliveConnectionResendRequestTest(read_failure);
}

TEST_F(HttpNetworkTransactionTest, KeepAliveConnectionEOF) {
  MockRead read_failure(false, OK);  // EOF
  KeepAliveConnectionResendRequestTest(read_failure);
}

TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionReset) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead(true, ERR_CONNECTION_RESET),
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, OK),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_CONNECTION_RESET, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

// What do various browsers do when the server closes a non-keepalive
// connection without sending any response header or body?
//
// IE7: error page
// Safari 3.1.2 (Windows): error page
// Firefox 3.0.1: blank page
// Opera 9.52: after five attempts, blank page
// Us with WinHTTP: error page (ERR_INVALID_RESPONSE)
// Us: error page (EMPTY_RESPONSE)
TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionEOF) {
  MockRead data_reads[] = {
    MockRead(false, OK),  // EOF
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(ERR_EMPTY_RESPONSE, out.rv);
}

// Test the request-challenge-retry sequence for basic auth.
// (basic auth is the easiest to mock, because it has no randomness).
TEST_F(HttpNetworkTransactionTest, BasicAuth) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, ERR_FAILED),
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  StaticMockSocket data2(data_reads2, data_writes2);
  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAlive) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with no response body to drain.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAliveNoBody) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// connection and with a large response body to drain.
TEST_F(HttpNetworkTransactionTest, BasicAuthKeepAliveLargeBody) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// Test the request-challenge-retry sequence for basic auth, over a keep-alive
// proxy connection, when setting up an SSL tunnel.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyKeepAlive) {
  // Configure against proxy server "myproxy:70".
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),

    // After calling trans->RestartWithAuth(), this is the request we should
    // be issuing -- the final header line contains the credentials.
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n"
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
    MockRead(false, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  // Wrong password (should be "bar").
  rv = trans->RestartWithAuth(L"foo", L"baz", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);
}

// Test that we don't read the response body when we fail to establish a tunnel,
// even if the user cancels the proxy's auth attempt.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyCancelTunnel) {
  // Configure against proxy server "myproxy:70".
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 407.
  MockRead data_reads[] = {
    MockRead("HTTP/1.1 407 Proxy Authentication Required\r\n"),
    MockRead("Proxy-Authenticate: Basic realm=\"MyRealm1\"\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(false, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  EXPECT_TRUE(response->headers->IsKeepAlive());
  EXPECT_EQ(407, response->headers->response_code());
  EXPECT_EQ(10, response->headers->GetContentLength());
  EXPECT_TRUE(HttpVersion(1, 1) == response->headers->GetHttpVersion());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);
}

void HttpNetworkTransactionTest::ConnectStatusHelperWithExpectedStatus(
    const MockRead& status, int expected_status) {
  // Configure against proxy server "myproxy:70".
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead data_reads[] = {
    status,
    MockRead("Content-Length: 10\r\n\r\n"),
    // No response body because the test stops reading here.
    MockRead(false, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(expected_status, rv);
}

void HttpNetworkTransactionTest::ConnectStatusHelper(const MockRead& status) {
  ConnectStatusHelperWithExpectedStatus(
      status, ERR_TUNNEL_CONNECTION_FAILED);
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
      ERR_PROXY_AUTH_REQUESTED);
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
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));

  // Configure against proxy server "myproxy:70".
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      CreateSession(&session_deps),
      &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, ERR_FAILED),
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
    MockRead(false, ERR_FAILED),  // Won't be reached.
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  StaticMockSocket data2(data_reads2, data_writes2);
  StaticMockSocket data3(data_reads3, data_writes3);
  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);
  session_deps.socket_factory.AddMockSocket(&data3);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"myproxy:70", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback2;

  rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
  EXPECT_EQ(L"basic", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(L"foo2", L"bar2", &callback3);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(100, response->headers->GetContentLength());
}

// The NTLM authentication unit tests were generated by capturing the HTTP
// requests and responses using Fiddler 2 and inspecting the generated random
// bytes in the debugger.

// Enter the correct password and authenticate successfully.
TEST_F(HttpNetworkTransactionTest, NTLMAuth1) {
  HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom1,
                                                         MockGetHostName);
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, ERR_UNEXPECTED),
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
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  StaticMockSocket data2(data_reads2, data_writes2);
  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"172.22.68.17:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  rv = trans->RestartWithAuth(L"testing-ntlm", L"testing-ntlm", &callback3);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}

// Enter a wrong password, and then the correct one.
TEST_F(HttpNetworkTransactionTest, NTLMAuth2) {
  HttpAuthHandlerNTLM::ScopedProcSetter proc_setter(MockGenerateRandom2,
                                                         MockGetHostName);
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, ERR_UNEXPECTED),
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
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Wrong password.
    MockRead("HTTP/1.1 401 Access Denied\r\n"),
    MockRead("WWW-Authenticate: Negotiate\r\n"),
    MockRead("WWW-Authenticate: NTLM\r\n"),
    MockRead("Connection: close\r\n"),
    MockRead("Content-Length: 42\r\n"),
    MockRead("Content-Type: text/html\r\n\r\n"),
    // Missing content -- won't matter, as connection will be reset.
    MockRead(false, ERR_UNEXPECTED),
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
    MockRead("Content-Type: text/html\r\n\r\n"),
    MockRead("You are not authorized to view this page\r\n"),

    // Lastly we get the desired content.
    MockRead("HTTP/1.1 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=utf-8\r\n"),
    MockRead("Content-Length: 13\r\n\r\n"),
    MockRead("Please Login\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  StaticMockSocket data2(data_reads2, data_writes2);
  StaticMockSocket data3(data_reads3, data_writes3);
  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);
  session_deps.socket_factory.AddMockSocket(&data3);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"172.22.68.17:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback3;

  // Enter the wrong password.
  rv = trans->RestartWithAuth(L"testing-ntlm", L"wrongpassword", &callback3);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback3.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback4;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback4);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback4.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // The password prompt info should have been set in response->auth_challenge.
  EXPECT_FALSE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(L"172.22.68.17:80", response->auth_challenge->host_and_port);
  EXPECT_EQ(L"", response->auth_challenge->realm);
  EXPECT_EQ(L"ntlm", response->auth_challenge->scheme);

  TestCompletionCallback callback5;

  // Now enter the right password.
  rv = trans->RestartWithAuth(L"testing-ntlm", L"testing-ntlm", &callback5);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback5.WaitForResult();
  EXPECT_EQ(OK, rv);

  response = trans->GetResponseInfo();
  EXPECT_TRUE(response->auth_challenge.get() == NULL);
  EXPECT_EQ(13, response->headers->GetContentLength());
}

// Test reading a server response which has only headers, and no body.
// After some maximum number of bytes is consumed, the transaction should
// fail with ERR_RESPONSE_HEADERS_TOO_BIG.
TEST_F(HttpNetworkTransactionTest, LargeHeadersNoBody) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, OK),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_RESPONSE_HEADERS_TOO_BIG, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);
}

// Make sure that we don't try to reuse a TCPClientSocket when failing to
// establish tunnel.
// http://code.google.com/p/chromium/issues/detail?id=3772
TEST_F(HttpNetworkTransactionTest, DontRecycleTCPSocketForSSLTunnel) {
  // Configure against proxy server "myproxy:70".
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));

  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  // Since we have proxy, should try to establish tunnel.
  MockWrite data_writes1[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  // The proxy responds to the connect with a 404, using a persistent
  // connection. Usually a proxy would return 501 (not implemented),
  // or 200 (tunnel established).
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 404 Not Found\r\n"),
    MockRead("Content-Length: 10\r\n\r\n"),
    MockRead(false, ERR_UNEXPECTED),  // Should not be reached.
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  session_deps.socket_factory.AddMockSocket(&data1);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(ERR_TUNNEL_CONNECTION_FAILED, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response == NULL);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  MessageLoop::current()->RunAllPending();

  // We now check to make sure the TCPClientSocket was not added back to
  // the pool.
  EXPECT_EQ(0, session->connection_pool()->IdleSocketCount());
  trans.reset();
  MessageLoop::current()->RunAllPending();
  // Make sure that the socket didn't get recycled after calling the destructor.
  EXPECT_EQ(0, session->connection_pool()->IdleSocketCount());
}

// Make sure that we recycle a socket after reading all of the response body.
TEST_F(HttpNetworkTransactionTest, RecycleSocket) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  MockRead data_reads[] = {
    // A part of the response body is received with the response headers.
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\nhel"),
    // The rest of the response body is received in two parts.
    MockRead("lo"),
    MockRead(" world"),
    MockRead("junk"),  // Should not be read!!
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  std::string status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 200 OK", status_line);

  EXPECT_EQ(0, session->connection_pool()->IdleSocketCount());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("hello world", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  MessageLoop::current()->RunAllPending();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, session->connection_pool()->IdleSocketCount());
}

// Make sure that we recycle a socket after a zero-length response.
// http://crbug.com/9880
TEST_F(HttpNetworkTransactionTest, RecycleSocketAfterZeroContentLength) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));

  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      session.get(), &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/csi?v=3&s=web&action=&"
                     "tran=undefined&ei=mAXcSeegAo-SMurloeUN&"
                     "e=17259,18167,19592,19773,19981,20133,20173,20233&"
                     "rt=prt.2642,ol.2649,xjs.2951");
  request.load_flags = 0;

  MockRead data_reads[] = {
    MockRead("HTTP/1.1 204 No Content\r\n"
             "Content-Length: 0\r\n"
             "Content-Type: text/html\r\n\r\n"),
    MockRead("junk"),  // Should not be read!!
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_TRUE(response != NULL);

  EXPECT_TRUE(response->headers != NULL);
  std::string status_line = response->headers->GetStatusLine();
  EXPECT_EQ("HTTP/1.1 204 No Content", status_line);

  EXPECT_EQ(0, session->connection_pool()->IdleSocketCount());

  std::string response_data;
  rv = ReadTransaction(trans.get(), &response_data);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("", response_data);

  // Empty the current queue.  This is necessary because idle sockets are
  // added to the connection pool asynchronously with a PostTask.
  MessageLoop::current()->RunAllPending();

  // We now check to make sure the socket was added back to the pool.
  EXPECT_EQ(1, session->connection_pool()->IdleSocketCount());
}

TEST_F(HttpNetworkTransactionTest, ResendRequestOnWriteBodyError) {
  HttpRequestInfo request[2];
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
  request[1].upload_data = new UploadData;
  request[1].upload_data->AppendBytes("foo", 3);
  request[1].load_flags = 0;

  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session = CreateSession(&session_deps);

  // The first socket is used for transaction 1 and the first attempt of
  // transaction 2.

  // The response of transaction 1.
  MockRead data_reads1[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 11\r\n\r\n"),
    MockRead("hello world"),
    MockRead(false, OK),
  };
  // The mock write results of transaction 1 and the first attempt of
  // transaction 2.
  MockWrite data_writes1[] = {
    MockWrite(false, 64),  // GET
    MockWrite(false, 93),  // POST
    MockWrite(false, ERR_CONNECTION_ABORTED),  // POST data
  };
  StaticMockSocket data1(data_reads1, data_writes1);

  // The second socket is used for the second attempt of transaction 2.

  // The response of transaction 2.
  MockRead data_reads2[] = {
    MockRead("HTTP/1.1 200 OK\r\nContent-Length: 7\r\n\r\n"),
    MockRead("welcome"),
    MockRead(false, OK),
  };
  // The mock write results of the second attempt of transaction 2.
  MockWrite data_writes2[] = {
    MockWrite(false, 93),  // POST
    MockWrite(false, 3),  // POST data
  };
  StaticMockSocket data2(data_reads2, data_writes2);

  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);

  const char* kExpectedResponseData[] = {
    "hello world", "welcome"
  };

  for (int i = 0; i < 2; ++i) {
    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(session, &session_deps.socket_factory));

    TestCompletionCallback callback;

    int rv = trans->Start(&request[i], &callback);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_TRUE(response != NULL);

    EXPECT_TRUE(response->headers != NULL);
    EXPECT_EQ("HTTP/1.1 200 OK", response->headers->GetStatusLine());

    std::string response_data;
    rv = ReadTransaction(trans.get(), &response_data);
    EXPECT_EQ(OK, rv);
    EXPECT_EQ(kExpectedResponseData[i], response_data);
  }
}

// Test the request-challenge-retry sequence for basic auth when there is
// an identity in the URL. The request should be sent as normal, but when
// it fails the identity from the URL is used to answer the challenge.
TEST_F(HttpNetworkTransactionTest, AuthIdentityInUrl) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
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
    MockRead(false, ERR_FAILED),
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
    MockRead(false, OK),
  };

  StaticMockSocket data1(data_reads1, data_writes1);
  StaticMockSocket data2(data_reads2, data_writes2);
  session_deps.socket_factory.AddMockSocket(&data1);
  session_deps.socket_factory.AddMockSocket(&data2);

  TestCompletionCallback callback1;

  int rv = trans->Start(&request, &callback1);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback1.WaitForResult();
  EXPECT_EQ(OK, rv);

  EXPECT_TRUE(trans->IsReadyToRestartForAuth());
  TestCompletionCallback callback2;
  rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback2.WaitForResult();
  EXPECT_EQ(OK, rv);
  EXPECT_FALSE(trans->IsReadyToRestartForAuth());

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  // There is no challenge info, since the identity in URL worked.
  EXPECT_TRUE(response->auth_challenge.get() == NULL);

  EXPECT_EQ(100, response->headers->GetContentLength());

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}

// Test that previously tried username/passwords for a realm get re-used.
TEST_F(HttpNetworkTransactionTest, BasicAuthCacheAndPreauth) {
  SessionDependencies session_deps;
  scoped_refptr<HttpNetworkSession> session = CreateSession(&session_deps);

  // Transaction 1: authenticate (foo, bar) on MyRealm1
  {
    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        session, &session_deps.socket_factory));

    HttpRequestInfo request;
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
      MockRead(false, ERR_FAILED),
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
      MockRead(false, OK),
    };

    StaticMockSocket data1(data_reads1, data_writes1);
    StaticMockSocket data2(data_reads2, data_writes2);
    session_deps.socket_factory.AddMockSocket(&data1);
    session_deps.socket_factory.AddMockSocket(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
    EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(L"foo", L"bar", &callback2);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 2: authenticate (foo2, bar2) on MyRealm2
  {
    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        session, &session_deps.socket_factory));

    HttpRequestInfo request;
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
      MockRead(false, ERR_FAILED),
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
      MockRead(false, OK),
    };

    StaticMockSocket data1(data_reads1, data_writes1);
    StaticMockSocket data2(data_reads2, data_writes2);
    session_deps.socket_factory.AddMockSocket(&data1);
    session_deps.socket_factory.AddMockSocket(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
    EXPECT_EQ(L"MyRealm2", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback2;

    rv = trans->RestartWithAuth(L"foo2", L"bar2", &callback2);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 3: Resend a request in MyRealm's protection space --
  // succeed with preemptive authorization.
  {
    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        session, &session_deps.socket_factory));

    HttpRequestInfo request;
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
      MockRead(false, OK),
    };

    StaticMockSocket data1(data_reads1, data_writes1);
    session_deps.socket_factory.AddMockSocket(&data1);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 4: request another URL in MyRealm (however the
  // url is not known to belong to the protection space, so no pre-auth).
  {
    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        session, &session_deps.socket_factory));

    HttpRequestInfo request;
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
      MockRead(false, ERR_FAILED),
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
      MockRead(false, OK),
    };

    StaticMockSocket data1(data_reads1, data_writes1);
    StaticMockSocket data2(data_reads2, data_writes2);
    session_deps.socket_factory.AddMockSocket(&data1);
    session_deps.socket_factory.AddMockSocket(&data2);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }

  // ------------------------------------------------------------------------

  // Transaction 5: request a URL in MyRealm, but the server rejects the
  // cached identity. Should invalidate and re-prompt.
  {
    scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
        session, &session_deps.socket_factory));

    HttpRequestInfo request;
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
      MockRead(false, ERR_FAILED),
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
      MockRead(false, ERR_FAILED),
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
      MockRead(false, OK),
    };

    StaticMockSocket data1(data_reads1, data_writes1);
    StaticMockSocket data2(data_reads2, data_writes2);
    StaticMockSocket data3(data_reads3, data_writes3);
    session_deps.socket_factory.AddMockSocket(&data1);
    session_deps.socket_factory.AddMockSocket(&data2);
    session_deps.socket_factory.AddMockSocket(&data3);

    TestCompletionCallback callback1;

    int rv = trans->Start(&request, &callback1);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback1.WaitForResult();
    EXPECT_EQ(OK, rv);

    EXPECT_TRUE(trans->IsReadyToRestartForAuth());
    TestCompletionCallback callback2;
    rv = trans->RestartWithAuth(std::wstring(), std::wstring(), &callback2);
    EXPECT_EQ(ERR_IO_PENDING, rv);
    rv = callback2.WaitForResult();
    EXPECT_EQ(OK, rv);
    EXPECT_FALSE(trans->IsReadyToRestartForAuth());

    const HttpResponseInfo* response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);

    // The password prompt info should have been set in
    // response->auth_challenge.
    EXPECT_FALSE(response->auth_challenge.get() == NULL);

    EXPECT_EQ(L"www.google.com:80", response->auth_challenge->host_and_port);
    EXPECT_EQ(L"MyRealm1", response->auth_challenge->realm);
    EXPECT_EQ(L"basic", response->auth_challenge->scheme);

    TestCompletionCallback callback3;

    rv = trans->RestartWithAuth(L"foo3", L"bar3", &callback3);
    EXPECT_EQ(ERR_IO_PENDING, rv);

    rv = callback3.WaitForResult();
    EXPECT_EQ(OK, rv);

    response = trans->GetResponseInfo();
    EXPECT_FALSE(response == NULL);
    EXPECT_TRUE(response->auth_challenge.get() == NULL);
    EXPECT_EQ(100, response->headers->GetContentLength());
  }
}

// Test the ResetStateForRestart() private method.
TEST_F(HttpNetworkTransactionTest, ResetStateForRestart) {
  // Create a transaction (the dependencies aren't important).
  SessionDependencies session_deps;
  scoped_ptr<HttpNetworkTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  // Setup some state (which we expect ResetStateForRestart() will clear).
  trans->header_buf_->Realloc(10);
  trans->header_buf_capacity_ = 10;
  trans->header_buf_len_ = 3;
  trans->header_buf_body_offset_ = 11;
  trans->header_buf_http_offset_ = 0;
  trans->response_body_length_ = 100;
  trans->response_body_read_ = 1;
  trans->read_buf_ = new IOBuffer(15);
  trans->read_buf_len_ = 15;
  trans->request_headers_->headers_ = "Authorization: NTLM";
  trans->request_headers_bytes_sent_ = 3;

  // Setup state in response_
  trans->response_.auth_challenge = new AuthChallengeInfo();
  trans->response_.ssl_info.cert_status = -15;
  trans->response_.response_time = base::Time::Now();
  trans->response_.was_cached = true;  // (Wouldn't ever actually be true...)

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
  EXPECT_EQ(NULL, trans->header_buf_->headers());
  EXPECT_EQ(0, trans->header_buf_capacity_);
  EXPECT_EQ(0, trans->header_buf_len_);
  EXPECT_EQ(-1, trans->header_buf_body_offset_);
  EXPECT_EQ(-1, trans->header_buf_http_offset_);
  EXPECT_EQ(-1, trans->response_body_length_);
  EXPECT_EQ(0, trans->response_body_read_);
  EXPECT_EQ(NULL, trans->read_buf_.get());
  EXPECT_EQ(0, trans->read_buf_len_);
  EXPECT_EQ("", trans->request_headers_->headers_);
  EXPECT_EQ(0U, trans->request_headers_bytes_sent_);
  EXPECT_EQ(NULL, trans->response_.auth_challenge.get());
  EXPECT_EQ(NULL, trans->response_.headers.get());
  EXPECT_EQ(false, trans->response_.was_cached);
  EXPECT_EQ(0, trans->response_.ssl_info.cert_status);
  EXPECT_FALSE(trans->response_.vary_data.is_valid());
}

// Test HTTPS connections to a site with a bad certificate
TEST_F(HttpNetworkTransactionTest, HTTPSBadCertificate) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

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

  StaticMockSocket ssl_bad_certificate;
  StaticMockSocket data(data_reads, data_writes);
  MockSSLSocket ssl_bad(true, ERR_CERT_AUTHORITY_INVALID);
  MockSSLSocket ssl(true, OK);

  session_deps.socket_factory.AddMockSocket(&ssl_bad_certificate);
  session_deps.socket_factory.AddMockSocket(&data);
  session_deps.socket_factory.AddMockSSLSocket(&ssl_bad);
  session_deps.socket_factory.AddMockSSLSocket(&ssl);

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
  SessionDependencies session_deps(CreateFixedProxyService("myproxy:70"));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  MockWrite proxy_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
  };

  MockRead proxy_reads[] = {
    MockRead("HTTP/1.0 200 Connected\r\n\r\n"),
    MockRead(false, OK)
  };

  MockWrite data_writes[] = {
    MockWrite("CONNECT www.google.com:443 HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Proxy-Connection: keep-alive\r\n\r\n"),
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

  StaticMockSocket ssl_bad_certificate(proxy_reads, proxy_writes);
  StaticMockSocket data(data_reads, data_writes);
  MockSSLSocket ssl_bad(true, ERR_CERT_AUTHORITY_INVALID);
  MockSSLSocket ssl(true, OK);

  session_deps.socket_factory.AddMockSocket(&ssl_bad_certificate);
  session_deps.socket_factory.AddMockSocket(&data);
  session_deps.socket_factory.AddMockSSLSocket(&ssl_bad);
  session_deps.socket_factory.AddMockSSLSocket(&ssl);

  TestCompletionCallback callback;

  for (int i = 0; i < 2; i++) {
    session_deps.socket_factory.ResetNextMockIndexes();

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(
            CreateSession(&session_deps),
            &session_deps.socket_factory));

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

TEST_F(HttpNetworkTransactionTest, BuildRequest_UserAgent) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.user_agent = "Chromium Ultra Awesome X Edition";

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "User-Agent: Chromium Ultra Awesome X Edition\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_Referer) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;
  request.referrer = GURL("http://the.previous.site.com/");

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Referer: http://the.previous.site.com/\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_PostContentLengthZero) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "POST";
  request.url = GURL("http://www.google.com/");

  MockWrite data_writes[] = {
    MockWrite("POST / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_PutContentLengthZero) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "PUT";
  request.url = GURL("http://www.google.com/");

  MockWrite data_writes[] = {
    MockWrite("PUT / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_HeadContentLengthZero) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "HEAD";
  request.url = GURL("http://www.google.com/");

  MockWrite data_writes[] = {
    MockWrite("HEAD / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Content-Length: 0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_CacheControlNoCache) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = LOAD_BYPASS_CACHE;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Pragma: no-cache\r\n"
              "Cache-Control: no-cache\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest,
       BuildRequest_CacheControlValidateCache) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = LOAD_VALIDATE_CACHE;

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "Cache-Control: max-age=0\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, BuildRequest_ExtraHeaders) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.extra_headers = "FooHeader: Bar\r\n";

  MockWrite data_writes[] = {
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n"
              "FooHeader: Bar\r\n\r\n"),
  };

  // Lastly, the server responds with the actual content.
  MockRead data_reads[] = {
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n"),
    MockRead("Content-Length: 100\r\n\r\n"),
    MockRead(false, OK),
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);
}

TEST_F(HttpNetworkTransactionTest, SOCKS4_HTTP_GET) {
  SessionDependencies session_deps;
  session_deps.proxy_service.reset(CreateFixedProxyService(
      "socks4://myproxy:1080"));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

  char write_buffer[] = { 0x04, 0x01, 0x00, 0x50, 127, 0, 0, 1, 0 };
  char read_buffer[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
    MockWrite(true, write_buffer, 9),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockWrite(true, read_buffer, 8),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(false, OK)
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

TEST_F(HttpNetworkTransactionTest, SOCKS4_SSL_GET) {
  SessionDependencies session_deps;
  session_deps.proxy_service.reset(CreateFixedProxyService(
      "socks4://myproxy:1080"));

  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("https://www.google.com/");
  request.load_flags = 0;

  unsigned char write_buffer[] = { 0x04, 0x01, 0x01, 0xBB, 127, 0, 0, 1, 0 };
  unsigned char read_buffer[] = { 0x00, 0x5A, 0x00, 0x00, 0, 0, 0, 0 };

  MockWrite data_writes[] = {
    MockWrite(true, reinterpret_cast<char*>(write_buffer), 9),
    MockWrite("GET / HTTP/1.1\r\n"
              "Host: www.google.com\r\n"
              "Connection: keep-alive\r\n\r\n")
  };

  MockRead data_reads[] = {
    MockWrite(true, reinterpret_cast<char*>(read_buffer), 8),
    MockRead("HTTP/1.0 200 OK\r\n"),
    MockRead("Content-Type: text/html; charset=iso-8859-1\r\n\r\n"),
    MockRead("Payload"),
    MockRead(false, OK)
  };

  StaticMockSocket data(data_reads, data_writes);
  session_deps.socket_factory.AddMockSocket(&data);

  MockSSLSocket ssl(true, OK);
  session_deps.socket_factory.AddMockSSLSocket(&ssl);

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(OK, rv);

  const HttpResponseInfo* response = trans->GetResponseInfo();
  EXPECT_FALSE(response == NULL);

  std::string response_text;
  rv = ReadTransaction(trans.get(), &response_text);
  EXPECT_EQ(OK, rv);
  EXPECT_EQ("Payload", response_text);
}

// Tests that for connection endpoints the group names are correctly set.
TEST_F(HttpNetworkTransactionTest, GroupNameForProxyConnections) {
  const struct {
    const std::string proxy_server;
    const std::string url;
    const std::string expected_group_name;
  } tests[] = {
    {
      "",  // no proxy (direct)
      "http://www.google.com/direct",
      "http://www.google.com/",
    },
    {
      "http_proxy",
      "http://www.google.com/http_proxy_normal",
      "proxy/http_proxy:80/",
    },
    {
      "socks4://socks_proxy:1080",
      "http://www.google.com/socks4_direct",
      "proxy/socks4://socks_proxy:1080/http://www.google.com/",
    },

    // SSL Tests
    {
      "",
      "https://www.google.com/direct_ssl",
      "https://www.google.com/",
    },
    {
      "http_proxy",
      "https://www.google.com/http_connect_ssl",
      "proxy/http_proxy:80/https://www.google.com/",
    },
    {
      "socks4://socks_proxy:1080",
      "https://www.google.com/socks4_ssl",
      "proxy/socks4://socks_proxy:1080/https://www.google.com/",
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    SessionDependencies session_deps;
    session_deps.proxy_service.reset(CreateFixedProxyService(
        tests[i].proxy_server));

    scoped_refptr<CaptureGroupNameSocketPool> conn_pool(
        new CaptureGroupNameSocketPool());

    scoped_refptr<HttpNetworkSession> session(CreateSession(&session_deps));
    session->connection_pool_ = conn_pool.get();

    scoped_ptr<HttpTransaction> trans(
        new HttpNetworkTransaction(
            session.get(),
            &session_deps.socket_factory));

    HttpRequestInfo request;
    request.method = "GET";
    request.url = GURL(tests[i].url);
    request.load_flags = 0;

    TestCompletionCallback callback;

    // We do not complete this request, the dtor will clean the transaction up.
    EXPECT_EQ(ERR_IO_PENDING, trans->Start(&request, &callback));
    EXPECT_EQ(tests[i].expected_group_name,
              conn_pool->last_group_name_received());
  }
}

TEST_F(HttpNetworkTransactionTest, ReconsiderProxyAfterFailedConnection) {
  scoped_refptr<RuleBasedHostMapper> host_mapper(new RuleBasedHostMapper());
  ScopedHostMapper scoped_host_mapper(host_mapper.get());
  host_mapper->AddSimulatedFailure("*");

  SessionDependencies session_deps(
      CreateFixedProxyService("myproxy:70;foobar:80"));
  scoped_ptr<HttpTransaction> trans(
      new HttpNetworkTransaction(
          CreateSession(&session_deps),
          &session_deps.socket_factory));

  HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");

  TestCompletionCallback callback;

  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);

  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

// Host resolution observer used by
// HttpNetworkTransactionTest.ResolveMadeWithReferrer to check that host
// resovle requests are issued with a referrer of |expected_referrer|.
class ResolutionReferrerObserver : public HostResolver::Observer {
 public:
  explicit ResolutionReferrerObserver(const GURL& expected_referrer)
      : expected_referrer_(expected_referrer),
        called_start_with_referrer_(false),
        called_finish_with_referrer_(false) {
  }

  virtual void OnStartResolution(int id,
                                 const HostResolver::RequestInfo& info) {
    if (info.referrer() == expected_referrer_)
      called_start_with_referrer_ = true;
  }

  virtual void OnFinishResolutionWithStatus(
      int id, bool was_resolved, const HostResolver::RequestInfo& info ) {
    if (info.referrer() == expected_referrer_)
      called_finish_with_referrer_ = true;
  }

  virtual void OnCancelResolution(int id,
                                  const HostResolver::RequestInfo& info ) {
    FAIL() << "Should not be cancelling any requests!";
  }

  bool did_complete_with_expected_referrer() const {
    return called_start_with_referrer_ && called_finish_with_referrer_;
  }

 private:
  GURL expected_referrer_;
  bool called_start_with_referrer_;
  bool called_finish_with_referrer_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionReferrerObserver);
};

// Make sure that when HostResolver::Resolve() is invoked, it passes through
// the "referrer". This is depended on by the DNS prefetch observer.
TEST_F(HttpNetworkTransactionTest, ResolveMadeWithReferrer) {
  GURL referrer = GURL("http://expected-referrer/");
  EXPECT_TRUE(referrer.is_valid());
  ResolutionReferrerObserver resolution_observer(referrer);

  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      CreateSession(&session_deps), &session_deps.socket_factory));

  // Attach an observer to watch the host resolutions being made.
  session_deps.host_resolver.AddObserver(&resolution_observer);

  // Connect up a mock socket which will fail when reading.
  MockRead data_reads[] = {
    MockRead(false, ERR_FAILED),
  };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  // Issue a request, containing an HTTP referrer.
  HttpRequestInfo request;
  request.method = "GET";
  request.referrer = referrer;
  request.url = GURL("http://www.google.com/");

  // Run the request until it fails reading from the socket.
  TestCompletionCallback callback;
  int rv = trans->Start(&request, &callback);
  EXPECT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();
  EXPECT_EQ(ERR_FAILED, rv);

  // Check that the host resolution observer saw |referrer|.
  EXPECT_TRUE(resolution_observer.did_complete_with_expected_referrer());
}

// Make sure that when the load flags contain LOAD_BYPASS_CACHE, the resolver's
// host cache is bypassed.
TEST_F(HttpNetworkTransactionTest, BypassHostCacheOnRefresh) {
  SessionDependencies session_deps;
  scoped_ptr<HttpTransaction> trans(new HttpNetworkTransaction(
      CreateSession(&session_deps), &session_deps.socket_factory));

  // Warm up the host cache so it has an entry for "www.google.com" (by doing
  // a synchronous lookup.)
  AddressList addrlist;
  int rv = session_deps.host_resolver.Resolve(
    HostResolver::RequestInfo("www.google.com", 80), &addrlist, NULL, NULL);
  EXPECT_EQ(OK, rv);

  // Verify that it was added to host cache, by doing a subsequent async lookup
  // and confirming it completes synchronously.
  TestCompletionCallback resolve_callback;
  rv = session_deps.host_resolver.Resolve(
      HostResolver::RequestInfo("www.google.com", 80), &addrlist,
      &resolve_callback, NULL);
  EXPECT_EQ(OK, rv);

  // Inject a failure the next time that "www.google.com" is resolved. This way
  // we can tell if the next lookup hit the cache, or the "network".
  // (cache --> success, "network" --> failure).
  scoped_refptr<RuleBasedHostMapper> host_mapper(new RuleBasedHostMapper());
  ScopedHostMapper scoped_host_mapper(host_mapper.get());
  host_mapper->AddSimulatedFailure("www.google.com");

  // Connect up a mock socket which will fail with ERR_UNEXPECTED during the
  // first read -- this won't be reached as the host resolution will fail first.
  MockRead data_reads[] = { MockRead(false, ERR_UNEXPECTED) };
  StaticMockSocket data(data_reads, NULL);
  session_deps.socket_factory.AddMockSocket(&data);

  // Issue a request, asking to bypass the cache(s).
  HttpRequestInfo request;
  request.method = "GET";
  request.load_flags = LOAD_BYPASS_CACHE;
  request.url = GURL("http://www.google.com/");

  // Run the request.
  TestCompletionCallback callback;
  rv = trans->Start(&request, &callback);
  ASSERT_EQ(ERR_IO_PENDING, rv);
  rv = callback.WaitForResult();

  // If we bypassed the cache, we would have gotten a failure while resolving
  // "www.google.com".
  EXPECT_EQ(ERR_NAME_NOT_RESOLVED, rv);
}

}  // namespace net
