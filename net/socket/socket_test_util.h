// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_SOCKET_TEST_UTIL_H_
#define NET_SOCKET_SOCKET_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/base/address_list.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/ssl_client_socket.h"

namespace net {

class ClientSocket;
class SSLClientSocket;

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

struct MockWriteResult {
  MockWriteResult(bool async, int result) : async(async), result(result) {}

  bool async;
  int result;
};

class MockSocket {
 public:
  MockSocket() {}

  virtual ~MockSocket() {}
  virtual MockRead GetNextRead() = 0;
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset() = 0;

  MockConnect connect_data() const { return connect_; }

 private:
  MockConnect connect_;

  DISALLOW_COPY_AND_ASSIGN(MockSocket);
};

// MockSocket which responds based on static tables of mock reads and writes.
class StaticMockSocket : public MockSocket {
 public:
  StaticMockSocket() : reads_(NULL), read_index_(0),
      writes_(NULL), write_index_(0) {}
  StaticMockSocket(MockRead* r, MockWrite* w) : reads_(r), read_index_(0),
      writes_(w), write_index_(0) {}

  // MockSocket methods:
  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data);
  virtual void Reset();

 private:
  MockRead* reads_;
  int read_index_;
  MockWrite* writes_;
  int write_index_;

  DISALLOW_COPY_AND_ASSIGN(StaticMockSocket);
};

// MockSocket which can make decisions about next mock reads based on
// received writes. It can also be used to enforce order of operations,
// for example that tested code must send the "Hello!" message before
// receiving response. This is useful for testing conversation-like
// protocols like FTP.
class DynamicMockSocket : public MockSocket {
 public:
  DynamicMockSocket();

  // MockSocket methods:
  virtual MockRead GetNextRead();
  virtual MockWriteResult OnWrite(const std::string& data) = 0;
  virtual void Reset();

  int short_read_limit() const { return short_read_limit_; }
  void set_short_read_limit(int limit) { short_read_limit_ = limit; }

 protected:
  // The next time there is a read from this socket, it will return |data|.
  // Before calling SimulateRead next time, the previous data must be consumed.
  void SimulateRead(const char* data);

 private:
  MockRead read_;
  bool has_read_;
  bool consumed_read_;

  // Max number of bytes we will read at a time. 0 means no limit.
  int short_read_limit_;

  DISALLOW_COPY_AND_ASSIGN(DynamicMockSocket);
};

// MockSSLSockets only need to keep track of the return code from calls to
// Connect().
struct  MockSSLSocket {
  MockSSLSocket(bool async, int result) : connect(async, result) { }

  MockConnect connect;
};

// Holds an array of Mock{SSL,}Socket elements.  As Mock{TCP,SSL}ClientSocket
// objects get instantiated, they take their data from the i'th element of this
// array.
template<typename T>
class MockSocketArray {
 public:
  MockSocketArray() : next_index_(0) {
  }

  T* GetNext() {
    DCHECK(next_index_ < sockets_.size());
    return sockets_[next_index_++];
  }

  void Add(T* socket) {
    DCHECK(socket);
    sockets_.push_back(socket);
  }

  void ResetNextIndex() {
    next_index_ = 0;
  }

 private:
  // Index of the next |sockets| element to use. Not an iterator because those
  // are invalidated on vector reallocation.
  size_t next_index_;

  // Mock sockets to be returned.
  std::vector<T*> sockets_;
};

// ClientSocketFactory which contains arrays of sockets of each type.
// You should first fill the arrays using AddMock{SSL,}Socket. When the factory
// is asked to create a socket, it takes next entry from appropriate array.
// You can use ResetNextMockIndexes to reset that next entry index for all mock
// socket types.
class MockClientSocketFactory : public ClientSocketFactory {
 public:
  void AddMockSocket(MockSocket* socket);
  void AddMockSSLSocket(MockSSLSocket* socket);
  void ResetNextMockIndexes();

  // ClientSocketFactory
  virtual ClientSocket* CreateTCPClientSocket(const AddressList& addresses);
  virtual SSLClientSocket* CreateSSLClientSocket(
      ClientSocket* transport_socket,
      const std::string& hostname,
      const SSLConfig& ssl_config);

 private:
  MockSocketArray<MockSocket> mock_sockets_;
  MockSocketArray<MockSSLSocket> mock_ssl_sockets_;
};

class MockClientSocket : public net::SSLClientSocket {
 public:
  MockClientSocket();

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback) = 0;

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info);
  virtual void GetSSLCertRequestInfo(
      net::SSLCertRequestInfo* cert_request_info);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback) = 0;
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback) = 0;

#if defined(OS_LINUX)
  virtual int GetPeerName(struct sockaddr *name, socklen_t *namelen);
#endif

 protected:
  void RunCallbackAsync(net::CompletionCallback* callback, int result);
  void RunCallback(int result);

  ScopedRunnableMethodFactory<MockClientSocket> method_factory_;
  net::CompletionCallback* callback_;
  bool connected_;
};

class MockTCPClientSocket : public MockClientSocket {
 public:
  MockTCPClientSocket(const net::AddressList& addresses,
                      net::MockSocket* socket);

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback);

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

 private:
  net::MockSocket* data_;
  int read_offset_;
  net::MockRead read_data_;
  bool need_read_data_;
};

class MockSSLClientSocket : public MockClientSocket {
 public:
  MockSSLClientSocket(
      net::ClientSocket* transport_socket,
      const std::string& hostname,
      const net::SSLConfig& ssl_config,
      net::MockSSLSocket* socket);
  ~MockSSLClientSocket();

  virtual void GetSSLInfo(net::SSLInfo* ssl_info);

  virtual int Connect(net::CompletionCallback* callback);
  virtual void Disconnect();

  // Socket methods:
  virtual int Read(net::IOBuffer* buf, int buf_len,
                   net::CompletionCallback* callback);
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    net::CompletionCallback* callback);

 private:
  class ConnectCallback;

  scoped_ptr<ClientSocket> transport_;
  net::MockSSLSocket* data_;
};

}  // namespace net

#endif  // NET_SOCKET_SOCKET_TEST_UTIL_H_
