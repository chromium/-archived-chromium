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
#include "net/proxy/proxy_resolver_fixed.h"
#include "testing/gtest/include/gtest/gtest.h"

//-----------------------------------------------------------------------------


struct MockConnect {
  // Asynchronous connection success.
  MockConnect() : async(true), result(net::OK) { }

  bool async;
  int result;
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

// MockWrite uses the same member fields as MockRead, but with different
// meanings. The expected input to MockTCPClientSocket::Write() is given
// by {data, data_len}, and the return value of Write() is controlled by
// {async, result}.
typedef MockRead MockWrite;

struct MockSocket {
  MockSocket() : reads(NULL), writes(NULL) { }

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
        write_index_(0),
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
      return net::ERR_IO_PENDING;
    }
    return result;
  }
  virtual int Write(const char* buf, int buf_len,
                    net::CompletionCallback* callback) {
    DCHECK(!callback_);
    // Not using mock writes; succeed synchronously.
    if (!data_->writes)
      return buf_len;

    // Check that what we are writing matches the expectation.
    // Then give the mocked return value.
    MockWrite& w = data_->writes[write_index_];
    int result = w.result;
    if (w.data) {
      std::string expected_data(w.data, w.data_len);
      std::string actual_data(buf, buf_len);
      EXPECT_EQ(expected_data, actual_data);
      if (expected_data != actual_data)
        return net::ERR_UNEXPECTED;
      if (result == net::OK)
        result = w.data_len;
    }
    if (w.async) {
      RunCallbackAsync(callback, result);
      return net::ERR_IO_PENDING;
    }
    return result;
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
  int write_index_;
  bool connected_;
};

class MockClientSocketFactory : public net::ClientSocketFactory {
 public:
  virtual net::ClientSocket* CreateTCPClientSocket(
      const net::AddressList& addresses) {
    return new MockTCPClientSocket(addresses);
  }
  virtual net::SSLClientSocket* CreateSSLClientSocket(
      net::ClientSocket* transport_socket,
      const std::string& hostname,
      const net::SSLConfig& ssl_config) {
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

// TODO(eroman): google style disallows default arguments...
net::HttpNetworkSession* CreateSession(
    net::ProxyResolver* proxy_resolver = NULL) {
  if (!proxy_resolver)
    proxy_resolver = new NullProxyResolver();
  return new net::HttpNetworkSession(proxy_resolver);
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
  int rv;
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

  out.rv = callback.WaitForResult();
  if (out.rv != net::OK)
    return out;

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
// Us: error page (net::EMPTY_RESPONSE)
TEST_F(HttpNetworkTransactionTest, NonKeepAliveConnectionEOF) {
  MockRead data_reads[] = {
    MockRead(false, net::OK),  // EOF
    MockRead("HTTP/1.0 200 OK\r\n\r\n"),  // Should not be used
    MockRead("hello world"),
    MockRead(false, net::OK),
  };
  SimpleGetHelperResult out = SimpleGetHelper(data_reads);
  EXPECT_EQ(out.rv, net::ERR_EMPTY_RESPONSE);
}

// Test the request-challenge-retry sequence for basic auth.
// (basic auth is the easiest to mock, because it has no randomness).
TEST_F(HttpNetworkTransactionTest, BasicAuth) {
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(), &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

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

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}

// Test the flow when both the proxy server AND origin server require
// authentication. Again, this uses basic auth for both since that is
// the simplest to mock.
TEST_F(HttpNetworkTransactionTest, BasicAuthProxyThenServer) {
  net::ProxyInfo proxy_info;
  proxy_info.UseNamedProxy("myproxy:70");

  // Configure against proxy server "myproxy:70".
  net::HttpTransaction* trans = new net::HttpNetworkTransaction(
      CreateSession(new net::ProxyResolverFixed(proxy_info)),
      &mock_socket_factory);

  net::HttpRequestInfo request;
  request.method = "GET";
  request.url = GURL("http://www.google.com/");
  request.load_flags = 0;

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

  trans->Destroy();

  // Empty the current queue.
  MessageLoop::current()->RunAllPending();
}
