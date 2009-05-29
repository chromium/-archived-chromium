// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/socket_test_util.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/io_buffer.h"
#include "net/base/socket.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockClientSocket : public net::SSLClientSocket {
 public:
  MockClientSocket();

  // ClientSocket methods:
  virtual int Connect(net::CompletionCallback* callback) = 0;

  // SSLClientSocket methods:
  virtual void GetSSLInfo(net::SSLInfo* ssl_info);
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
  int read_index_;
  int read_offset_;
  int write_index_;
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

MockClientSocket::MockClientSocket()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      callback_(NULL),
      connected_(false) {
}

void MockClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  NOTREACHED();
}

void MockClientSocket::Disconnect() {
  connected_ = false;
  callback_ = NULL;
}

bool MockClientSocket::IsConnected() const {
  return connected_;
}

bool MockClientSocket::IsConnectedAndIdle() const {
  return connected_;
}

#if defined(OS_LINUX)
int MockClientSocket::GetPeerName(struct sockaddr *name, socklen_t *namelen) {
  memset(reinterpret_cast<char *>(name), 0, *namelen);
  return net::OK;
}
#endif  // defined(OS_LINUX)

void MockClientSocket::RunCallbackAsync(net::CompletionCallback* callback,
                                        int result) {
  callback_ = callback;
  MessageLoop::current()->PostTask(FROM_HERE,
      method_factory_.NewRunnableMethod(
          &MockClientSocket::RunCallback, result));
}

void MockClientSocket::RunCallback(int result) {
  net::CompletionCallback* c = callback_;
  callback_ = NULL;
  if (c)
    c->Run(result);
}

MockTCPClientSocket::MockTCPClientSocket(const net::AddressList& addresses,
                                         net::MockSocket* socket)
    : data_(socket),
      read_index_(0),
      read_offset_(0),
      write_index_(0) {
  DCHECK(data_);
}

int MockTCPClientSocket::Connect(net::CompletionCallback* callback) {
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

int MockTCPClientSocket::Read(net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  DCHECK(!callback_);
  net::MockRead& r = data_->reads[read_index_];
  int result = r.result;
  if (r.data) {
    if (r.data_len - read_offset_ > 0) {
      result = std::min(buf_len, r.data_len - read_offset_);
      memcpy(buf->data(), r.data + read_offset_, result);
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

int MockTCPClientSocket::Write(net::IOBuffer* buf, int buf_len,
                               net::CompletionCallback* callback) {
  DCHECK(buf);
  DCHECK(buf_len > 0);
  DCHECK(!callback_);
  // Not using mock writes; succeed synchronously.
  if (!data_->writes)
    return buf_len;

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  net::MockWrite& w = data_->writes[write_index_++];
  int result = w.result;
  if (w.data) {
    std::string expected_data(w.data, w.data_len);
    std::string actual_data(buf->data(), buf_len);
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

class MockSSLClientSocket::ConnectCallback :
    public net::CompletionCallbackImpl<MockSSLClientSocket::ConnectCallback> {
 public:
  ConnectCallback(MockSSLClientSocket *ssl_client_socket,
                  net::CompletionCallback* user_callback,
                  int rv)
      : ALLOW_THIS_IN_INITIALIZER_LIST(
          net::CompletionCallbackImpl<MockSSLClientSocket::ConnectCallback>(
                this, &ConnectCallback::Wrapper)),
        ssl_client_socket_(ssl_client_socket),
        user_callback_(user_callback),
        rv_(rv) {
  }

 private:
  void Wrapper(int rv) {
    if (rv_ == net::OK)
      ssl_client_socket_->connected_ = true;
    user_callback_->Run(rv_);
    delete this;
  }

  MockSSLClientSocket* ssl_client_socket_;
  net::CompletionCallback* user_callback_;
  int rv_;
};

MockSSLClientSocket::MockSSLClientSocket(
    net::ClientSocket* transport_socket,
    const std::string& hostname,
    const net::SSLConfig& ssl_config,
    net::MockSSLSocket* socket)
    : transport_(transport_socket),
      data_(socket) {
  DCHECK(data_);
}

MockSSLClientSocket::~MockSSLClientSocket() {
  Disconnect();
}

void MockSSLClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  ssl_info->Reset();
}

int MockSSLClientSocket::Connect(net::CompletionCallback* callback) {
  DCHECK(!callback_);
  ConnectCallback* connect_callback = new ConnectCallback(
      this, callback, data_->connect.result);
  int rv = transport_->Connect(connect_callback);
  if (rv == net::OK) {
    delete connect_callback;
    if (data_->connect.async) {
      RunCallbackAsync(callback, data_->connect.result);
      return net::ERR_IO_PENDING;
    }
    if (data_->connect.result == net::OK)
      connected_ = true;
    return data_->connect.result;
  }
  return rv;
}

void MockSSLClientSocket::Disconnect() {
  MockClientSocket::Disconnect();
  if (transport_ != NULL)
    transport_->Disconnect();
}

int MockSSLClientSocket::Read(net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  DCHECK(!callback_);
  return transport_->Read(buf, buf_len, callback);
}

int MockSSLClientSocket::Write(net::IOBuffer* buf, int buf_len,
                               net::CompletionCallback* callback) {
  DCHECK(!callback_);
  return transport_->Write(buf, buf_len, callback);
}

}  // namespace

namespace net {

void MockClientSocketFactory::AddMockSocket(MockSocket* socket) {
  mock_sockets_.Add(socket);
}

void MockClientSocketFactory::AddMockSSLSocket(MockSSLSocket* socket) {
  mock_ssl_sockets_.Add(socket);
}

void MockClientSocketFactory::ResetNextMockIndexes() {
  mock_sockets_.ResetNextIndex();
  mock_ssl_sockets_.ResetNextIndex();
}

ClientSocket* MockClientSocketFactory::CreateTCPClientSocket(
    const AddressList& addresses) {
  return new MockTCPClientSocket(addresses, mock_sockets_.GetNext());
}

SSLClientSocket* MockClientSocketFactory::CreateSSLClientSocket(
    ClientSocket* transport_socket,
    const std::string& hostname,
    const SSLConfig& ssl_config) {
  return new MockSSLClientSocket(transport_socket, hostname, ssl_config,
                                 mock_ssl_sockets_.GetNext());
}

}  // namespace net
