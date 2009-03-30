// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_SOCKET_MAC_H_
#define NET_BASE_SSL_CLIENT_SOCKET_MAC_H_

#include <Security/Security.h>

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"

namespace net {

// An SSL client socket implemented with Secure Transport.
class SSLClientSocketMac : public SSLClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake. ssl_config specifies the SSL
  // settings.
  SSLClientSocketMac(ClientSocket* transport_socket,
                     const std::string& hostname,
                     const SSLConfig& ssl_config);
  ~SSLClientSocketMac();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

 private:
  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoConnect();
  int DoConnectComplete(int result);
  int DoPayloadRead();
  int DoPayloadWrite();
  int DoHandshake();
  int DoReadComplete(int result);
  void OnWriteComplete(int result);

  static OSStatus SSLReadCallback(SSLConnectionRef connection,
                                  void* data,
                                  size_t* data_length);
  static OSStatus SSLWriteCallback(SSLConnectionRef connection,
                                   const void* data,
                                   size_t* data_length);

  CompletionCallbackImpl<SSLClientSocketMac> io_callback_;
  CompletionCallbackImpl<SSLClientSocketMac> write_callback_;

  scoped_ptr<ClientSocket> transport_;
  std::string hostname_;
  SSLConfig ssl_config_;

  CompletionCallback* user_callback_;

  // Used by both Read and Write functions.
  char* user_buf_;
  int user_buf_len_;

  enum State {
    STATE_NONE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_PAYLOAD_READ,
    STATE_PAYLOAD_WRITE,
    STATE_HANDSHAKE,
    STATE_READ_COMPLETE,
  };
  State next_state_;
  State next_io_state_;

  int server_cert_status_;

  bool completed_handshake_;
  SSLContextRef ssl_context_;

  // These are buffers for holding data during I/O. The "slop" is the amount of
  // space at the ends of the receive buffer that are allocated for holding data
  // but don't (yet).
  std::vector<char> send_buffer_;
  int pending_send_error_;
  std::vector<char> recv_buffer_;
  int recv_buffer_head_slop_;
  int recv_buffer_tail_slop_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_SOCKET_MAC_H_
