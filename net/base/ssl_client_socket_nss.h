// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_SSL_CLIENT_SOCKET_NSS_H_
#define NET_BASE_SSL_CLIENT_SOCKET_NSS_H_

#include <nspr.h>
#include <nss.h>
#include <string>

#include "base/scoped_ptr.h"
#include "net/base/completion_callback.h"
#include "net/base/nss_memio.h"
#include "net/base/ssl_client_socket.h"
#include "net/base/ssl_config_service.h"

namespace net {

// An SSL client socket implemented with Mozilla NSS.
class SSLClientSocketNSS : public SSLClientSocket {
 public:
  // Takes ownership of the transport_socket, which may already be connected.
  // The given hostname will be compared with the name(s) in the server's
  // certificate during the SSL handshake.  ssl_config specifies the SSL
  // settings.
  SSLClientSocketNSS(ClientSocket* transport_socket,
                     const std::string& hostname,
                     const SSLConfig& ssl_config);
  ~SSLClientSocketNSS();

  // SSLClientSocket methods:
  virtual void GetSSLInfo(SSLInfo* ssl_info);

  // ClientSocket methods:
  virtual int Connect(CompletionCallback* callback);
  virtual int ReconnectIgnoringLastError(CompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;

  // Socket methods:
  virtual int Read(char* buf, int buf_len, CompletionCallback* callback);
  virtual int Write(const char* buf, int buf_len, CompletionCallback* callback);

 private:
  void DoCallback(int result);
  void OnIOComplete(int result);

  int DoLoop(int last_io_result);
  int DoConnect();
  int DoConnectComplete(int result);
  int DoHandshakeRead();
  int DoPayloadRead();
  int DoPayloadWrite();
  int Init();
  int BufferSend(void);
  int BufferRecv(void);
  void BufferSendComplete(int result);
  void BufferRecvComplete(int result);

  // nss calls this on error.  We pass 'this' as the first argument.
  static SECStatus OwnBadCertHandler(void* arg, PRFileDesc* socket);

  CompletionCallbackImpl<SSLClientSocketNSS> buffer_send_callback_;
  CompletionCallbackImpl<SSLClientSocketNSS> buffer_recv_callback_;
  bool transport_send_busy_;
  bool transport_recv_busy_;

  CompletionCallbackImpl<SSLClientSocketNSS> io_callback_;
  scoped_ptr<ClientSocket> transport_;
  std::string hostname_;
  SSLConfig ssl_config_;

  CompletionCallback* user_callback_;

  // Used by both Read and Write functions.
  char* user_buf_;
  int user_buf_len_;

  // Set when handshake finishes.  Value is net error code, see net_errors.h
  int server_cert_error_;

  bool completed_handshake_;

  enum State {
    STATE_NONE,
    STATE_CONNECT,
    STATE_CONNECT_COMPLETE,
    STATE_HANDSHAKE_READ,
    // No STATE_HANDSHAKE_READ_COMPLETE needed, go to STATE_NONE instead.
    STATE_PAYLOAD_WRITE,
    STATE_PAYLOAD_READ,
  };
  State next_state_;

  // The NSS SSL state machine
  PRFileDesc* nss_fd_;

  // Buffers for the network end of the SSL state machine
  memio_Private* nss_bufs_;

  static bool nss_options_initialized_;
};

}  // namespace net

#endif  // NET_BASE_SSL_CLIENT_SOCKET_NSS_H_

