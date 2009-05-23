// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SOCKET_TEST_UTIL_H_
#define NET_BASE_SOCKET_TEST_UTIL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/base/address_list.h"
#include "net/base/client_socket_factory.h"
#include "net/base/net_errors.h"
#include "net/base/ssl_config_service.h"

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

struct MockSocket {
  MockSocket() : reads(NULL), writes(NULL) { }
  MockSocket(MockRead* r, MockWrite* w) : reads(r), writes(w) { }

  MockConnect connect;
  MockRead* reads;
  MockWrite* writes;
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

}  // namespace net

#endif  // NET_BASE_SOCKET_TEST_UTIL_H_
