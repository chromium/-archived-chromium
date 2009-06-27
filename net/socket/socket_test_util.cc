// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/socket/socket_test_util.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "net/base/ssl_info.h"
#include "net/socket/socket.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

MockClientSocket::MockClientSocket()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      callback_(NULL),
      connected_(false) {
}

void MockClientSocket::GetSSLInfo(net::SSLInfo* ssl_info) {
  NOTREACHED();
}

void MockClientSocket::GetSSLCertRequestInfo(
    net::SSLCertRequestInfo* cert_request_info) {
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
      read_offset_(0),
      read_data_(true, net::ERR_UNEXPECTED),
      need_read_data_(true) {
  DCHECK(data_);
  data_->Reset();
}

int MockTCPClientSocket::Connect(net::CompletionCallback* callback) {
  DCHECK(!callback_);
  if (connected_)
    return net::OK;
  connected_ = true;
  if (data_->connect_data().async) {
    RunCallbackAsync(callback, data_->connect_data().result);
    return net::ERR_IO_PENDING;
  }
  return data_->connect_data().result;
}

int MockTCPClientSocket::Read(net::IOBuffer* buf, int buf_len,
                              net::CompletionCallback* callback) {
  DCHECK(!callback_);

  if (!IsConnected())
    return net::ERR_UNEXPECTED;

  if (need_read_data_) {
    read_data_ = data_->GetNextRead();
    need_read_data_ = false;
  }
  int result = read_data_.result;
  if (read_data_.data) {
    if (read_data_.data_len - read_offset_ > 0) {
      result = std::min(buf_len, read_data_.data_len - read_offset_);
      memcpy(buf->data(), read_data_.data + read_offset_, result);
      read_offset_ += result;
      if (read_offset_ == read_data_.data_len) {
        need_read_data_ = true;
        read_offset_ = 0;
      }
    } else {
      result = 0;  // EOF
    }
  }
  if (read_data_.async) {
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

  if (!IsConnected())
    return net::ERR_UNEXPECTED;

  std::string data(buf->data(), buf_len);
  net::MockWriteResult write_result = data_->OnWrite(data);

  if (write_result.async) {
    RunCallbackAsync(callback, write_result.result);
    return net::ERR_IO_PENDING;
  }
  return write_result.result;
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

MockRead StaticMockSocket::GetNextRead() {
  return reads_[read_index_++];
}

MockWriteResult StaticMockSocket::OnWrite(const std::string& data) {
  if (!writes_) {
    // Not using mock writes; succeed synchronously.
    return MockWriteResult(false, data.length());
  }

  // Check that what we are writing matches the expectation.
  // Then give the mocked return value.
  net::MockWrite* w = &writes_[write_index_++];
  int result = w->result;
  if (w->data) {
    std::string expected_data(w->data, w->data_len);
    EXPECT_EQ(expected_data, data);
    if (expected_data != data)
      return MockWriteResult(false, net::ERR_UNEXPECTED);
    if (result == net::OK)
      result = w->data_len;
  }
  return MockWriteResult(w->async, result);
}

void StaticMockSocket::Reset() {
  read_index_ = 0;
  write_index_ = 0;
}

DynamicMockSocket::DynamicMockSocket()
    : read_(false, ERR_UNEXPECTED),
      has_read_(false),
      short_read_limit_(0) {
}

MockRead DynamicMockSocket::GetNextRead() {
  if (!has_read_)
    return MockRead(true, ERR_UNEXPECTED);
  MockRead result = read_;
  if (short_read_limit_ == 0 || result.data_len <= short_read_limit_) {
    has_read_ = false;
  } else {
    result.data_len = short_read_limit_;
    read_.data += result.data_len;
    read_.data_len -= result.data_len;
  }
  return result;
}

void DynamicMockSocket::Reset() {
  has_read_ = false;
}

void DynamicMockSocket::SimulateRead(const char* data) {
  EXPECT_FALSE(has_read_) << "Unconsumed read: " << read_.data;
  read_ = MockRead(data);
  has_read_ = true;
}

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
